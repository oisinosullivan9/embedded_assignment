import serial
import struct
import time

def parse_packet(data):
    # Adjust the packet format to match the actual packet structure in your C code
    packet_format = '<BHI I f B B B H'  # Matches CommunicationPacket structure
    
    try:
        # Unpack the packet
        packet_type, payload_length, timestamp, free_heap, cpu_temp, cpu_usage, task_count, temp_status, crc = struct.unpack(packet_format, data)
        
        print(f"Packet Type: {packet_type}")
        print(f"Timestamp: {timestamp}")
        print(f"Free Heap: {free_heap} bytes")
        print(f"CPU Temp: {cpu_temp}°C")
        print(f"CPU Usage: {cpu_usage}")
        print(f"Task Count: {task_count}")
        print(f"Temp Status: {['Normal', 'Low', 'High'][temp_status]}")
        print("---")
    except struct.error as e:
        print(f"Error unpacking packet: {e}")

def main():
    try:
        # Try different COM ports if needed
        ser = serial.Serial('COM5', 115200, timeout=1)
        
        packet_size = struct.calcsize('<BHI I f B B B H')
        
        while True:
            # Read packet
            packet = ser.read(packet_size)
            
            if len(packet) == packet_size:
                parse_packet(packet)
            
            time.sleep(0.1)  # Small delay to prevent CPU overuse
    
    except serial.SerialException as e:
        print(f"Serial port error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    main()