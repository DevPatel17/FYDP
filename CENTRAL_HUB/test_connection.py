#!/usr/bin/env python3
import socket
import struct
import time
import threading

class VentCommunicator:
    def __init__(self):
        # Socket for receiving
        self.recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recv_socket.bind(('0.0.0.0', 5000))
        self.recv_socket.settimeout(1.0)  # Add 1 second timeout
        
        # Socket for sending
        self.send_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        # Store the phone's address when we receive a packet
        self.phone_address = None
        
        # Flag for stopping the threads
        self.running = True

    def send_packet(self, pkt_type, value):
        """Send a packet to the phone"""
        if self.phone_address is None:
            print("No phone address known yet")
            return
            
        # Create packet (same format as iOS app)
        packet = struct.pack('<If', pkt_type, value)
        try:
            # Send to port 3001 (matches iOS app's listening port)
            send_address = (self.phone_address[0], 3001)
            self.send_socket.sendto(packet, send_address)
            print(f'Sent packet - Type: {pkt_type}, Value: {value}')
        except Exception as e:
            print(f'Error sending packet: {e}')

    def receive_packets(self):
        # """Listen for incoming packets from the phone"""
        while self.running:
            try:
                print('\nWaiting for packet...')
                data, address = self.recv_socket.recvfrom(1024)
                
                # Store the phone's address for sending responses
                self.phone_address = address
                
                if len(data) >= 8:
                    pkt_type = struct.unpack('<I', data[0:4])[0]
                    value = struct.unpack('<f', data[4:8])[0]
                    
                    print(f'Received from {address}:')
                    print(f'  Type: {pkt_type}')
                    print(f'  Value: {value:.2f}')
                    
                    if pkt_type == 7:  # New vent setup request
                        print("Received vent setup request, simulating setup...")
                        time.sleep(2)  # Simulate setup time
                        self.send_packet(8, 1.0)  # Send success packet
                    elif pkt_type <= 5:  # If it's a regular vent command
                        # Simulate sending back current temperature
                        self.send_packet(pkt_type, 22.5)
                        
            except socket.timeout:
                # This is normal, just continue the loop
                continue
            except Exception as e:
                if self.running:  # Only print errors if we're still meant to be running
                    print(f'Error receiving packet: {e}')

    def start(self):
        """Start the communication threads"""
        print('Starting UDP server...')
        
        # Print IP address information
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 1))
            local_ip = s.getsockname()[0]
            print(f'Local IP address is: {local_ip}')
            print('Use this IP address in your iOS app')
            s.close()
        except Exception as e:
            print(f'Could not determine IP address: {e}')
        
        # Start receive thread
        self.recv_thread = threading.Thread(target=self.receive_packets)
        self.recv_thread.start()
        
        # Main loop for testing
        try:
            while True:
                time.sleep(5)  # Send a test packet every 5 seconds if we have a phone address
                # if self.phone_address and self.running:
                    # Example: send temperature updates
                    # for vent_id in range(6):
                    #     self.send_packet(vent_id, 22.5 + vent_id)
        except KeyboardInterrupt:
            print("\nShutting down...")
            self.stop()

    def stop(self):
        """Clean shutdown of the communicator"""
        print("Stopping communicator...")
        self.running = False
        
        # Close the sockets
        try:
            self.recv_socket.close()
            self.send_socket.close()
        except Exception as e:
            print(f"Error closing sockets: {e}")
        
        # Wait for receive thread to finish (with timeout)
        print("Waiting for threads to finish...")
        self.recv_thread.join(timeout=2.0)  # Wait up to 2 seconds
        if self.recv_thread.is_alive():
            print("Warning: Thread did not terminate cleanly")
        else:
            print("Cleanup completed successfully")

if __name__ == '__main__':
    communicator = VentCommunicator()
    communicator.start()