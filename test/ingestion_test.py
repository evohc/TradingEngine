import socket
import time

def build_fix_msg(fields):
    body = "".join(f"{k}={v}\x01" for k, v in fields.items())
    return body + "10=000\x01"

def main():
    host = '127.0.0.1'
    port = 8888

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        print(f"Connecting to {host}:{port}...")
        s.connect((host, port))
    except ConnectionRefusedError:
        print(f"Connection refused.")
        return

    #Send Logon (MsgType = A)
    logon_fields = {
        '8': 'FIX.4.4',
        '35': 'A',
        '34': '1',
        '49': 'Client1',
        '56': 'TradingExchangeDemo'
    }
    logon_msg = build_fix_msg(logon_fields)
    
    print(f"Sending Logon: {logon_msg.replace(chr(1), '|')}")
    s.sendall(logon_msg.encode('ascii'))
    
    # Brief pause to ensure logon is processed before orders arrive
    time.sleep(0.5)

    #Send 10 Orders (MsgType = D)
    for i in range(1, 11):
        order_fields = {
            '8': 'FIX.4.4',
            '35': 'D',
            '34': str(i),                 # MsgSeqNum (Matches expected sequence logic)
            '49': 'Client1',              # SenderCompID
            '56': 'TradingExchangeDemo',  # TargetCompID
            '11': f'ORD_999_{i}',         # ClientOrderID
            '55': 'SYM_1',                # Symbol
            '54': '1',                    # Side: 1=Buy
            '38': '10',                   # Quantity
            '44': '65000.50'              # Price
        }
        order_msg = build_fix_msg(order_fields)
        
        print(f"Sending Order {i}: {order_msg.replace(chr(1), '|')}")
        s.sendall(order_msg.encode('ascii'))
        
        # Small delay to simulate network spacing
        time.sleep(0.1) 
        
    print("Test complete.")
    s.close()

if __name__ == "__main__":
    main()