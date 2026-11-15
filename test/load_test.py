import socket
import time
import random

SOH = '\x01'

# this test generates 50,000 random bids and asks and make sure there is no crossed book. 
# A crossed book is when the best bid is higher than the lowest bid.

# EExample xpected output:

   #Market Data : SYM_4
   #Bids (qty @ price) |   Asks (price @ qty)      
   #---------------------------------------------
   #2  @  270.00       |   286.00 @ 15        
   #52 @  199.00       |   354.00 @ 4         
   #48 @  198.00       |   357.00 @ 21        
   #19 @  197.00       |   360.00 @ 45        
   #41 @  196.00       |   372.00 @ 26        
   #Market Data : SYM_1
   #Bids (qty @ price) |   Asks (price @ qty)      
   #---------------------------------------------
   #18 @  62870.00     |   63044.00 @ 34      
   #23 @  62524.00     |   63091.00 @ 13      
   #27 @  62083.00     |   63178.00 @ 49      
   #47 @  61369.00     |   63219.00 @ 39      
   #11 @  61280.00     |   63396.00 @ 20   
        
class StressTester:
    def __init__(self, host='127.0.0.1', port=8888):
        self.address = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.seq_num = 1
        self.sender_comp_id = "STRESS_TEST"
        self.target_comp_id = "TradingExchangeDemo"

    def format_fix(self, msg_type, body_fields):
        header = f"35={msg_type}|49={self.sender_comp_id}|56={self.target_comp_id}|34={self.seq_num}|"
        self.seq_num += 1
        body = "".join(f"{k}={v}|" for k, v in body_fields.items())
        msg_body = header + body
        body_length = len(msg_body)
        raw_msg = f"8=FIX.4.4|9={body_length}|{msg_body}10=000|"
        return raw_msg.replace('|', SOH)

    def run(self, num_orders=50000):
        print(f"Connecting to Gateway at {self.address}...")
        self.sock.connect(self.address)
        
        # Logon
        self.sock.sendall(self.format_fix("A", {"108": "30", "141": "Y"}).encode('ascii'))
        time.sleep(0.5)
        
        print(f"Preparing {num_orders} randomized orders for SYM_4 and SYM_1...")
        symbols = ["SYM_4", "SYM_1"]
        sides = ["1", "2"] # 1=Buy, 2=Sell
        
        buffer = bytearray()
        start_time = time.time()
        
        for i in range(num_orders):
            sym = random.choice(symbols)
            side = random.choice(sides)
            qty = str(random.randint(1, 50))
            
            # Keep prices clustered so they actually cross and match
            if sym == "SYM_4":
                price = str(random.randint(100, 500))
            else:
                price = str(random.randint(60000, 65000))
            
            fields = {
                "11": f"st_{i}", 
                "55": sym,           
                "54": side,             
                "38": qty,              
                "44": price             
            }
            
            fix_msg = self.format_fix("D", fields)
            buffer.extend(fix_msg.encode('ascii'))
            
            # Flush buffer to the TCP socket in batches of 1,000
            if i % 1000 == 0:
                self.sock.sendall(buffer)
                buffer.clear()
                
        # Flush remaining
        if buffer:
            self.sock.sendall(buffer)

        elapsed = time.time() - start_time
        print(f"Dispatched {num_orders} orders in {elapsed:.4f} seconds.")
        print(f"Throughput: {num_orders / elapsed:,.0f} messages/second")
        
        time.sleep(1)
        self.sock.close()

if __name__ == "__main__":
    input("Any key to continue...")
    tester = StressTester()
    tester.run(50000)