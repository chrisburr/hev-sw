#!/usr/bin/env python3
# data server to manage comms between UIs and LLI
#
# Author: Dónal Murray <donal.murray@cern.ch>

import asyncio
import json
import svpi
import time
import threading
from typing import List
import logging
logging.basicConfig(level=logging.INFO,
                    format='hevserver %(asctime)s - %(levelname)s - %(message)s')


class HEVServer(object):
    def __init__(self):
        self._alarms = []
        self._values = []
        self._settings = []
        self._polling = True
        self._broadcasting_period = 1
        self._broadcasting = True
        self._lock = threading.Lock()  # lock for the database

        # start worker thread to update values in background
        worker = threading.Thread(target=self.polling, daemon=True)
        worker.start()

    def __repr__(self):
        with self._lock:
            return f"alarms: {self._alarms}. sensor values: {self._values}"

    def polling(self) -> None:
        # get values in the background
        while self._polling:
            with self._lock:
                self._values = svpi.getValues()
                self._alarms = svpi.getAlarms()
                self._settings = svpi.getThresholds()
            time.sleep(self._broadcasting_period)

    async def handle_request(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        # listen for queries on the request socket
        data = await reader.read(300)
        request = json.loads(data.decode("utf-8"))
        addr = writer.get_extra_info("peername")
        payload = ""

        # three possible queries: set mode, set thresholds or both
        if request["type"] == "setmode":
            mode = request["mode"]
            logging.debug(f"{addr!r} requested to change to mode {mode!r}")

            # send via protocol and prepare reply
            if svpi.setMode(mode):
                packet = f"""{{"type": "ackmode", "mode": \"{mode}\"}}""".encode()
            else:
                packet = f"""{{"type": "nack"}}""".encode()
        elif request["type"] == "setthresholds":
            thresholds = request["thresholds"]
            logging.debug(
                f"{addr!r} requested to set thresholds to {thresholds!r}")

            # send via protocol
            payload = svpi.setThresholds(thresholds)
            # prepare reply
            packet = f"""{{"type": "ackthresholds", "thresholds": \"{payload}\"}}""".encode()
        elif request["type"] == "setup":
            mode = request["mode"]
            thresholds = request["thresholds"]
            logging.debug(f"{addr!r} requested to change to mode {mode!r}")
            logging.debug(
                f"{addr!r} requested to set thresholds to {thresholds!r}")

            # send via protocol and prepare reply
            if svpi.setMode(mode):
                svpi.setThresholds(thresholds)
                packet = f"""{{"type": "ack", "mode": \"{mode}\", "thresholds": \"{thresholds}\"}}""".encode()
            else:
                packet = f"""{{"type": "nack"}}""".encode()

        # send reply and close connection
        writer.write(packet)
        await writer.drain()
        writer.close()

    async def handle_broadcast(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        # log address
        addr = writer.get_extra_info("peername")
        logging.info(f"Broadcasting to {addr!r}")

        while self._broadcasting:
            # take lock of db and prepare packet
            with self._lock:
                payload: List[float] = self._values
                alarms: List[str] = self._alarms
            broadcast_packet = f"""{{ "type": "broadcast", "sensors": {payload}, "alarms": {str(alarms).replace("'",'"')} }}"""
            logging.debug(f"Send: {broadcast_packet}")

            try:
                writer.write(broadcast_packet.encode())
                await writer.drain()
                await asyncio.sleep(self._broadcasting_period)
            except (ConnectionResetError, BrokenPipeError):
                # Connection lost, stop trying to broadcast and free up socket
                logging.warning(f"Connection lost with {addr!r}")
                self._broadcasting = False

        self._broadcasting = True
        writer.close()

    async def serve_request(self, ip: str, port: int) -> None:
        server = await asyncio.start_server(
            self.handle_request, ip, port)

        # get address for log
        addr = server.sockets[0].getsockname()
        logging.info(f"Answering request from {addr}")

        async with server:
            await server.serve_forever()

    async def serve_broadcast(self, ip: str, port: int) -> None:
        server = await asyncio.start_server(
            self.handle_broadcast, ip, port)

        # get address for log
        addr = server.sockets[0].getsockname()
        logging.info(f"Serving on {addr}")

        async with server:
            await server.serve_forever()

    async def create_sockets(self) -> None:
        LOCALHOST = "127.0.0.1"
        b1 = self.serve_broadcast(LOCALHOST, 54320)  # WebUI broadcast
        r1 = self.serve_request(LOCALHOST, 54321)    # joint request socket
        b2 = self.serve_broadcast(LOCALHOST, 54322)  # NativeUI broadcast
        tasks = [b1, r1, b2]
        await asyncio.gather(*tasks, return_exceptions=True)

    def serve_all(self) -> None:
        asyncio.run(self.create_sockets())


if __name__ == "__main__":
    hevsrv = HEVServer()
    hevsrv.serve_all()
