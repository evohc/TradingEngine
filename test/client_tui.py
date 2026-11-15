import socket
import threading
import time
import sys

SOH = '\x01'

class FixClient:
    def __init__(self, host='127.0.0.1', port=8888):
        self.address = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.seq_num = 1
        self.sender_comp_id = "PYTHON_TEST"
        self.target_comp_id = "TradingExchangeDemo"
        self.running = True

    def format_fix(self, msg_type, body_fields):
        header = f"35={msg_type}|49={self.sender_comp_id}|56={self.target_comp_id}|34={self.seq_num}|"
        self.seq_num += 1
        
        body = "".join(f"{k}={v}|" for k, v in body_fields.items())
        msg_body = header + body
        
        body_length = len(msg_body)
        raw_msg = f"8=FIX.4.4|9={body_length}|{msg_body}10=000|"
        
        return raw_msg.replace('|', SOH)

    def parse_fix_to_dict(self, fix_str):
        fields = {}
        for pair in fix_str.split('|'):
            if '=' in pair:
                key, value = pair.split('=', 1)
                fields[key] = value
        return fields

    def connect_and_logon(self):
        print(f"Connecting to Gateway at {self.address}...")
        self.sock.connect(self.address)
        logon_msg = self.format_fix("A", {"108": "30", "141": "Y"})
        self.sock.sendall(logon_msg.encode('ascii'))
        print("Sent FIX Logon.\n")

    def listen_loop(self):
        recv_buffer = ""
        while self.running:
            try:
                data = self.sock.recv(4096)
                if not data:
                    print("\nDisconnected, gateway closed the connection.")
                    self.running = False
                    break
                
                recv_buffer += data.decode('ascii').replace(SOH, '|')
                
                while "10=000|" in recv_buffer:
                    msg_end = recv_buffer.find("10=000|") + 7
                    raw_str = recv_buffer[:msg_end]
                    recv_buffer = recv_buffer[msg_end:]
                    
                    if "35=8" in raw_str:
                        fields = self.parse_fix_to_dict(raw_str)
                        
                        order_id = fields.get("11", "UNKNOWN")
                        qty = fields.get("32", "0")
                        symbol = fields.get("55", "UNKNOWN")
                        
                        side_tag = fields.get("54", "0")
                        side_str = "BUY " if side_tag == "1" else ("SELL" if side_tag == "2" else "UNK ")
                        
                        raw_price = int(fields.get("31", "0"))
                        real_price = raw_price / 10000.0
                        
                        exec_type = fields.get("150", "0")
                        if exec_type == "1":
                            status = "🔶 PARTIAL FILL"
                        elif exec_type == "2":
                            status = "✅ FULL FILL   "
                        else:
                            status = f"🔄 STATUS ({exec_type}) "
                            
                        print(f"\n{status} | {side_str} | {symbol:<8} | Order: {order_id} | Traded: {qty} @ ${real_price:,.2f}")
                        print("Action (B=Buy, S=Sell, Q=Quit): ", end="", flush=True)
                    else:
                        print(f"\n[NETWORK IN] {raw_str}\nAction: ", end="", flush=True)
                    
            except Exception as e:
                if self.running:
                    print(f"\nSocket error: {e}")
                break

    def run(self):
        self.connect_and_logon()
        
        listener = threading.Thread(target=self.listen_loop, daemon=True)
        listener.start()
        
        order_id = 1000
        time.sleep(0.5) 
        
        print("==================================================")
        print(" Interactive FIX client ('Q' to Quit)")
        print(" Format:  <Side> <Qty> <Price> <Symbol>")
        print(" Example: B 1 65000 SYM_1")
        print(" Example: S 10 180 SYM_4")
        print("==================================================")

        try:
            while self.running:
                cmd = input("Action (B=Buy, S=Sell, Q=Quit): ").strip().upper()
                
                if cmd == 'Q':
                    break
                    
                if cmd.startswith('B') or cmd.startswith('S'):
                    parts = cmd.split()
                    if len(parts) != 4:
                        print("Invalid format. Use: B/S <qty> <price> <symbol>")
                        continue
                        
                    side = "1" if cmd.startswith('B') else "2"
                    qty = parts[1]
                    price = parts[2]
                    symbol = parts[3]
                    
                    fields = {
                        "11": f"order_{order_id}", 
                        "55": symbol,           
                        "54": side,             
                        "38": qty,              
                        "44": price             
                    }
                    order_id += 1
                    
                    fix_msg = self.format_fix("D", fields)
                    self.sock.sendall(fix_msg.encode('ascii'))
                    print(f"[TCP OUT] Sent Order order_{order_id-1}")
                    
        except KeyboardInterrupt:
            pass
            
        finally:
            self.running = False
            self.sock.close()
            print("\nShutting down client.")

if __name__ == "__main__":
    client = FixClient()
    client.run()