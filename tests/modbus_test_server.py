"""Simple Modbus TCP server for integration testing.
NO external dependencies. Supports FC 0x01,0x03,0x05,0x06,0x0F,0x10.
"""
import sys, socket, struct, threading

class ModbusTestServer:
    def __init__(self, host='127.0.0.1', port=1502):
        self.host = host; self.port = port
        self.holding_regs = [0]*256
        self.holding_regs[0]=0x022B; self.holding_regs[1]=0x0001; self.holding_regs[2]=0x0064
        self.coils = [False]*256
        self.coils[0:10] = [True]*10
        self.server_sock = None; self.running = False; self.thread = None

    def start(self):
        self.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_sock.bind((self.host, self.port))
        self.server_sock.listen(5)
        self.server_sock.settimeout(0.5)
        self.running = True
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()
        print(f"Modbus TCP test server listening on {self.host}:{self.port}")
        sys.stdout.flush()

    def stop(self):
        self.running = False
        if self.thread: self.thread.join(timeout=2)
        if self.server_sock: self.server_sock.close()

    def _serve(self):
        while self.running:
            try:
                conn, addr = self.server_sock.accept()
                t = threading.Thread(target=self._handle, args=(conn,), daemon=True)
                t.start()
            except socket.timeout: continue
            except Exception: break

    def _recv_all(self, conn, n):
        data = b''
        while len(data) < n:
            chunk = conn.recv(n - len(data))
            if not chunk: return None
            data += chunk
        return data

    def _handle(self, conn):
        try:
            conn.settimeout(5)
            while self.running:
                hdr = self._recv_all(conn, 7)
                if not hdr: break
                txn = struct.unpack('>H', hdr[0:2])[0]
                length = struct.unpack('>H', hdr[4:6])[0]
                if length < 2: break
                pdu = self._recv_all(conn, length - 1)
                if not pdu: break
                resp_pdu = self._process(pdu[0], pdu[1:])
                resp_len = 1 + len(resp_pdu)
                resp = struct.pack('>HHHB', txn, 0, resp_len, hdr[6]) + resp_pdu
                conn.sendall(resp)
        except Exception: pass
        finally: conn.close()

    def _process(self, fc, data):
        if fc == 0x03: return self._read_hr(data)
        if fc == 0x06: return self._write_sr(data)
        if fc == 0x01: return self._read_co(data)
        if fc == 0x05: return self._write_sc(data)
        if fc == 0x10: return self._write_mr(data)
        if fc == 0x0F: return self._write_mc(data)
        return bytes([fc | 0x80, 0x01])

    def _read_hr(self, data):
        if len(data) < 4: return bytes([0x83, 0x03])
        start = struct.unpack('>H', data[0:2])[0]; qty = struct.unpack('>H', data[2:4])[0]
        if qty < 1 or qty > 125 or start+qty > 256: return bytes([0x83, 0x02])
        resp = struct.pack('>BB', 0x03, qty*2)
        for i in range(qty): resp += struct.pack('>H', self.holding_regs[start+i])
        return resp

    def _write_sr(self, data):
        if len(data) < 4: return bytes([0x86, 0x03])
        addr = struct.unpack('>H', data[0:2])[0]; val = struct.unpack('>H', data[2:4])[0]
        if addr < 256: self.holding_regs[addr] = val
        return bytes([0x06]) + data

    def _read_co(self, data):
        if len(data) < 4: return bytes([0x81, 0x03])
        start = struct.unpack('>H', data[0:2])[0]; qty = struct.unpack('>H', data[2:4])[0]
        if qty < 1 or qty > 2000 or start+qty > 256: return bytes([0x81, 0x02])
        bc = (qty+7)//8; resp = bytes([0x01, bc])
        for b in range(bc):
            v = 0
            for bit in range(8):
                idx = start+b*8+bit
                if idx < 256 and self.coils[idx]: v |= (1<<bit)
            resp += bytes([v])
        return resp

    def _write_sc(self, data):
        if len(data) < 4: return bytes([0x85, 0x03])
        addr = struct.unpack('>H', data[0:2])[0]; val = struct.unpack('>H', data[2:4])[0]
        if addr < 256: self.coils[addr] = (val == 0xFF00)
        return bytes([0x05]) + data

    def _write_mr(self, data):
        if len(data) < 5: return bytes([0x90, 0x03])
        start = struct.unpack('>H', data[0:2])[0]; qty = struct.unpack('>H', data[2:4])[0]
        if data[4] != qty*2 or start+qty > 256: return bytes([0x90, 0x02])
        for i in range(qty):
            self.holding_regs[start+i] = struct.unpack('>H', data[5+i*2:7+i*2])[0]
        return bytes([0x10]) + data[0:4]

    def _write_mc(self, data):
        if len(data) < 5: return bytes([0x8F, 0x03])
        start = struct.unpack('>H', data[0:2])[0]; qty = struct.unpack('>H', data[2:4])[0]
        for i in range(qty):
            if start+i < 256:
                byte_idx, bit_idx = i//8, i%8
                self.coils[start+i] = bool((data[5+byte_idx] >> bit_idx) & 1)
        return bytes([0x0F]) + data[0:4]

def run_server(port=1502):
    srv = ModbusTestServer(port=port)
    srv.start()
    try:
        import time
        while True: time.sleep(1)
    except KeyboardInterrupt: pass
    finally: srv.stop()

if __name__ == '__main__':
    run_server(int(sys.argv[1]) if len(sys.argv) > 1 else 1502)
