#!/usr/bin/env python3
import socket
import struct
import time
import threading
import asyncio
from bleak import BleakScanner, BleakClient
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# The device name we're looking for (same as in your ESP32 code)
DEVICE_NAME = "BLE-Server"

# The UUIDs from your ESP32 code
READ_UUID = "00000180-0000-1000-8000-00805f9b34fb"    # Service UUID
WRITE_UUID = "0000dead-0000-1000-8000-00805f9b34fb"   # Write characteristic UUID
READ_CHAR_UUID = "0000fef4-0000-1000-8000-00805f9b34fb"  # Read characteristic UUID

async def notification_handler(sender, data):
    """Handle incoming notifications from the BLE device"""
    logger.info(f"Received data: {data.decode()}")

async def find_device():
    """Scan for the ESP32 device"""
    logger.info(f"Scanning for {DEVICE_NAME}...")
    
    while True:
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name == DEVICE_NAME:
                logger.info(f"Found {DEVICE_NAME}: {device.address}")
                return device
        logger.info("Device not found, scanning again...")
        await asyncio.sleep(1)

async def connect_to_ble_client(VentID, phone_connection):
    try:
        device = await find_device()
        
        # Define notification callback
        def notification_handler(sender, data):
            message = data.decode()
            logger.info(f"Received notification: {message}")
            
            if message == "Connected":
                phone_connection.send_packet(1, str(VentID)) # Send packet to phone to let it know the vent is connected
            else:
                try:
                    # Parse message in format "ID: X, temp: Y"
                    if "ID:" in message and "temp:" in message:
                        # Split message into parts and extract values
                        parts = message.split(',')
                        vent_id = int(parts[0].split(':')[1].strip())
                        temp = float(parts[1].split(':')[1].strip())
                        
                        logger.info(f"Parsed - Vent ID: {vent_id}, Temperature: {temp}")
                        
                        # Send type 2 packet with combined vent ID and temperature
                        value_str = f"{vent_id}.{temp}"
                        phone_connection.send_packet(2, value_str)                   
                except Exception as e:
                    logger.error(f"Error parsing message: {e}")
            
        async with BleakClient(device.address) as client:
            logger.info(f"Connected to {device.name}")

            # Enable notifications for the characteristic
            await client.start_notify(READ_CHAR_UUID, notification_handler)
            logger.info("Notifications enabled")

            # Write data
            message = f"Connected, Vent ID: {VentID}".encode()
            await client.write_gatt_char(WRITE_UUID, message)
            logger.info(f"Sent message: {message.decode()}")
            
            # Wait for notification in a loop
            # while True:
            #     await asyncio.sleep(1)

            return client   
    except Exception as e:
        logger.error(f"Error: {str(e)}")
        
        

    except Exception as e:
        logger.error(f"Error: {str(e)}")
        if 'client' in locals():
            await client.disconnect()
        return None

class VentCover:
    # Class variable to keep track of the next available ventID
    next_vent_id = 1
    
    def __init__(self, temperature=0.0, vent_cover_status=0, user_forced=False, vent_id=None):
        self.temperature = float(temperature)
        self.vent_cover_status = int(vent_cover_status)
        self.user_forced = bool(user_forced)
        self.ble_connection = None  # Default to no connection
        # If no vent_id provided, assign the next available one
        if vent_id is None:
            self.vent_id = VentCover.next_vent_id
            VentCover.next_vent_id += 1
        else:
            self.vent_id = int(vent_id)
    
    def set_ble_connection(self, connection):
        """Set the BLE connection for this vent"""
        self.ble_connection = connection
    
    def get_ble_connection(self):
        """Get the current BLE connection"""
        return self.ble_connection
    
    def is_connected(self):
        """Check if the vent has an active BLE connection"""
        return self.ble_connection is not None
    
    def disconnect(self):
        """Remove the BLE connection"""
        self.ble_connection = None
    
    def __str__(self):
        connected_status = "Connected" if self.is_connected() else "Not Connected"
        return f"VentCover(id={self.vent_id}, temperature={self.temperature}, status={self.vent_cover_status}, user_forced={self.user_forced}, BLE: {connected_status})"
    
class VentCoverSystem:
    def __init__(self):
        self.vent_covers = []
    
    def add_vent_cover(self, temperature=0.0, vent_cover_status=0, user_forced=False):
        """Create and add a new vent cover to the system"""
        new_vent = VentCover(temperature, vent_cover_status, user_forced)
        self.vent_covers.append(new_vent)
        return new_vent.vent_id
    
    def remove_vent_cover(self, vent_id):
        """Remove a vent cover by its ID"""
        self.vent_covers = [vent for vent in self.vent_covers if vent.vent_id != vent_id]
    
    def get_vent_cover(self, vent_id):
        """Get a vent cover by its ID"""
        for vent in self.vent_covers:
            if vent.vent_id == vent_id:
                return vent
        return None
    
    def get_all_vent_covers(self):
        """Return all vent covers"""
        return self.vent_covers
    
    def update_vent_cover(self, vent_id, temperature=None, vent_cover_status=None, user_forced=None):
        """Update a vent cover's properties"""
        vent = self.get_vent_cover(vent_id)
        if vent:
            if temperature is not None:
                vent.temperature = float(temperature)
            if vent_cover_status is not None:
                vent.vent_cover_status = int(vent_cover_status)
            if user_forced is not None:
                vent.user_forced = bool(user_forced)
            return True
        return False
    
    def __str__(self):
        return "\n".join(str(vent) for vent in self.vent_covers)


vent_system = VentCoverSystem()

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
            
        # Pack the packet type as a 32-bit integer
        packet = struct.pack('<I', pkt_type)
        
        # If value is a string, encode it and append to packet
        if isinstance(value, str):
            packet += value.encode('utf-8')
        else:
            # Keep old float packing for backwards compatibility
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
                # print('\nWaiting for packet...')
                data, address = self.recv_socket.recvfrom(1024)
                
                # Store the phone's address for sending responses
                self.phone_address = address
                # VentConnection = BleakClient()
                
                if len(data) >= 8:
                    pkt_type = struct.unpack('<I', data[0:4])[0]
                    value = struct.unpack('<f', data[4:8])[0]
                    
                    print(f'Received from {address}:')
                    print(f'  Type: {pkt_type}')
                    print(f'  Value: {value:.2f}')
                    
                    if pkt_type == 1:  # New vent setup request
                        print("Received vent setup request, starting setup...")
                        
                        # Setup a new vent cover and store it's VentID
                        # By Default, user_forced is false, temp = 0, vent is open
                        VentID = vent_system.add_vent_cover()
                        
                        # TODO: Put this in a new thread
                        # Look for ESP32, send it it's associated VentID, wait for accepted message, send iPhone messaged that setup was completed
                        try:
                            VentConnection = asyncio.run(connect_to_ble_client(VentID, self))
                            # Store BLE connection
                            vent_system.get_vent_cover(VentID).set_ble_connection(VentConnection)
                        except KeyboardInterrupt:
                            logger.info("Script terminated by user")
                        except Exception as e:
                            logger.error(f"Unexpected error: {str(e)}")
                        
                        
                    # elif pkt_type <= 5:  # If it's a regular vent command
                    #     # Simulate sending back current temperature
                    #     self.send_packet(pkt_type, 22.5)
                        
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