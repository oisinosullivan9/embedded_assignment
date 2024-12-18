import socket

# Replace with laptop's IP and the port number
HOST = "192.168.113.61"  # change to your laptop's IP
PORT = 12345

def main():
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((HOST, PORT))
    print(f"Listening on {HOST}:{PORT}")

    try:
        while True:
            data, addr = sock.recvfrom(1024)  # Buffer size is 1024 bytes
            print(f"Received from {addr}: {data.decode('utf-8')}")
    except KeyboardInterrupt:
        print("Exiting...")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
