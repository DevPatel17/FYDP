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
DEVICE_NAME1 = "BLE-Server"
DEVICE_NAME2 = "BLE-Server-2"

# The UUIDs from your ESP32 code
READ_UUID = "00000180-0000-1000-8000-00805f9b34fb"    # Service UUID
WRITE_UUID = "0000dead-0000-1000-8000-00805f9b34fb"   # Write characteristic UUID
READ_CHAR_UUID = "0000fef4-0000-1000-8000-00805f9b34fb"  # Read characteristic UUID

# Create a global event loop to be shared across threads
loop = asyncio.new_event_loop()

async def find_device(device_name):
    """Scan for the ESP32 device"""
    logger.info(f"Scanning for {device_name}...")
    
    while True:
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name == device_name:
                logger.info(f"Found {device.name}: {device.address}")
                return device
        logger.info("Device not found, scanning again...")
        await asyncio.sleep(1)

async def connect_to_ble_client(device_name):
    """
    Connects to the BLE device and returns the client object
    without setting up notifications or sending messages
    """
    try:
        device = await find_device(device_name)
        client = BleakClient(device.address)
        await client.connect()
        logger.info(f"Connected to {device.name}")
        return client, device
    except Exception as e:
        logger.error(f"Error connecting to BLE device: {str(e)}")
        return None, None

async def run_ble_connection(client, VentID, phone_connection):
    """
    Async function to handle BLE connection notifications and messages
    """
    if not client:
        logger.error("No client provided")
        return

    try:
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
                        
                        #TODO Put new temperature into PID controller here
                        vent = vent_system.get_vent_cover(vent_id)
                        
                        if float(temp) < float(vent.temperature) + 0.1 and float(temp) > float(vent.temperature) - 0.1:
                            temp_change = False
                        else:
                            temp_change = True
                        
                        if(vent.user_forced == True and temp_change == True):
                            new_pos = int(vent.PIDController.compute(temp, vent.desired_temp))
                            logger.info(f"Sending new vent position: {new_pos}")
                            send_vent_position(vent, str(new_pos))
                        
                        vent.temperature = temp
                    elif "ID:" in message and "motor:" in message:
                        parts = message.split(',')
                        vent_id = int(parts[0].split(':')[1].strip())
                        motor_pos = float(parts[1].split(':')[1].strip())
                        
                        logger.info(f"Parsed - Vent ID: {vent_id}, Motor position: {motor_pos}")
                        
                        # Send type 3 packet with combined vent ID and motor position
                        value_str = f"{vent_id}.motor{motor_pos}"
                        phone_connection.send_packet(3, value_str) 
                        vent.user_forced == False
                        
                                        
                except Exception as e:
                    logger.error(f"Error parsing message: {e}")

        # Enable notifications for the characteristic
        await client.start_notify(READ_CHAR_UUID, notification_handler)
        logger.info("Notifications enabled")

        # Write initial connection message
        message = f"Connected, Vent ID: {VentID}".encode()
        await client.write_gatt_char(WRITE_UUID, message)
        logger.info(f"Sent message: {message.decode()}")
        
        # Keep the connection alive
        while True:
            await asyncio.sleep(1)

    except Exception as e:
        logger.error(f"Error in BLE connection: {str(e)}")
    finally:
        try:
            await client.disconnect()
        except:
            pass

def ble_connection_thread(client, VentID, phone_connection):
    """
    Thread function to handle BLE connection
    This function sets up a new event loop for the thread
    """
    # Set the event loop for this thread
    asyncio.set_event_loop(loop)
    
    # Submit the task to the event loop
    future = asyncio.run_coroutine_threadsafe(
        run_ble_connection(client, VentID, phone_connection), 
        loop
    )
    
    # You can handle exceptions from the future if needed
    try:
        # This will block until the future completes
        future.result()
    except Exception as e:
        logger.error(f"Error in BLE connection thread: {str(e)}")

class VentPIDController:
    """
    A PID controller for regulating vent opening based on temperature difference.
    
    The controller takes current and desired temperature as inputs and
    outputs a value between 0-100, where:
    - 0 means the vent should be closed (temperatures match)
    - 100 means the vent should be fully open (large temperature difference)
    """
    
    def __init__(self, kp=1.5, ki=0.5, kd=0.05, min_output=0, max_output=100):
        """
        Initialize the PID controller with tuning parameters.
        
        Args:
            kp (float): Proportional gain coefficient
            ki (float): Integral gain coefficient  
            kd (float): Derivative gain coefficient
            min_output (float): Minimum output value (default: 0)
            max_output (float): Maximum output value (default: 100)
        """
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.min_output = min_output
        self.max_output = max_output
        
        # PID state variables
        self.integral = 0
        self.previous_error = 0
        self.last_time = None
        
    def reset(self):
        """Reset the controller's internal state."""
        self.integral = 0
        self.previous_error = 0
        self.last_time = None
        
    def compute(self, current_temp, desired_temp, dt=1.0):
        """
        Compute the control output based on temperature difference.
        
        Args:
            current_temp (float): Current temperature reading
            desired_temp (float): Desired target temperature
            dt (float): Time delta since last update in seconds (default: 1.0)
            
        Returns:
            float: Control output in range [min_output, max_output]
        """
        # For cooling: error is positive when current temp > desired temp
        # For heating: error would be reversed
        error = current_temp - desired_temp
        
        # Proportional term
        proportional = self.kp * error
        
        # Integral term - accumulates over time
        self.integral += error * dt
        integral_term = self.ki * self.integral
        
        # Derivative term - rate of change
        derivative = (error - self.previous_error) / dt if dt > 0 else 0
        derivative_term = self.kd * derivative
        
        # Save current error for next iteration
        self.previous_error = error
        
        # Calculate PID output
        output = proportional + integral_term + derivative_term
        
        # If we're in cooling mode, a positive output means open the vent
        # If we're in heating mode, we would handle direction differently
        
        # Clamp the output to the allowed range
        output = max(self.min_output, min(self.max_output, output))
        
        return output
    
    def update_tuning(self, kp=None, ki=None, kd=None):
        """
        Update the PID tuning parameters.
        
        Args:
            kp (float, optional): New proportional gain
            ki (float, optional): New integral gain
            kd (float, optional): New derivative gain
        """
        if kp is not None:
            self.kp = kp
        if ki is not None:
            self.ki = ki
        if kd is not None:
            self.kd = kd

class VentCover:
    # Class variable to keep track of the next available ventID
    next_vent_id = 1
    
    def __init__(self, temperature=0.0, vent_cover_status=0, user_forced=False, vent_id=None):
        self.temperature = float(temperature)
        self.desired_temp = float(temperature)
        self.vent_cover_status = int(vent_cover_status)
        self.user_forced = bool(user_forced)
        self.ble_connection = None  # Default to no connection
        self.ble_thread = None
        self.PIDController = VentPIDController()
        # If no vent_id provided, assign the next available one
        if vent_id is None:
            self.vent_id = VentCover.next_vent_id
            VentCover.next_vent_id += 1
        else:
            self.vent_id = int(vent_id)
    
    def set_ble_thread(self, connection):
        """Set the BLE thread for this vent"""
        self.ble_thread = connection
        
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

def send_vent_position(vent, position):
    """Send a position command to a vent over BLE"""
    if not vent.is_connected():
        print(f"Cannot send position to vent {vent.vent_id}: not connected")
        return
        
    # Create a future to execute the async BLE write operation
    async def send_position_command(client, position):
        try:
            # Encode the position command
            message = position.encode()
            await client.write_gatt_char(WRITE_UUID, message)
            logger.info(f"Sent position command to vent {vent.vent_id}: {position}")
            return True
        except Exception as e:
            logger.error(f"Error sending position to vent {vent.vent_id}: {str(e)}")
            return False
            
    # Submit the task to the event loop
    future = asyncio.run_coroutine_threadsafe(
        send_position_command(vent.get_ble_connection(), position),
        loop
    )
    
    try:
        # Wait for the operation to complete with a timeout
        result = future.result(timeout=5.0)
        if result:
            print(f"Successfully sent position {position} to vent {vent.vent_id}")
        else:
            print(f"Failed to send position to vent {vent.vent_id}")
    except Exception as e:
        print(f"Error in send_vent_position: {str(e)}")

class VentCommunicator:
    def __init__(self):
        # Socket for receiving
        self.recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recv_socket.bind(('0.0.0.0', 5001))
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
        while self.running:
            try:
                data, address = self.recv_socket.recvfrom(1024)
                
                # Store the phone's address for sending responses
                self.phone_address = address
                
                # if len(data) >= 8:
                pkt_type = struct.unpack('<I', data[0:4])[0]
                # Extract the string value after the packet type
                value_str = ""
                if len(data) > 4:
                    try:
                        # Try to decode the remaining bytes as a UTF-8 string
                        value_str = data[4:].decode('utf-8')
                    except Exception as e:
                        print(f"Error decoding string value: {e}")
                
                print(f'Received from {address}:')
                print(f'  Type: {pkt_type}')
                print(f'  Value: {value_str}')
                
                if pkt_type == 3:  # Vent position control request
                    try:
                        # Convert float value to string and parse as "VentID.VentPosition"
                        # value_str = str(value)
                        if '.' in value_str:
                            parts = value_str.split('.')
                            vent_id = int(parts[0])
                            vent_position = parts[1]
                            
                            print(f"Vent control request - Vent ID: {vent_id}, Position: {vent_position}")
                            
                            # Get the vent cover and check if it has a BLE connection
                            vent = vent_system.get_vent_cover(vent_id)
                            vent.user_forced = False
                            if vent and vent.is_connected():
                                # Send vent position command to the vent via BLE
                                send_vent_position(vent, vent_position)
                            else:
                                print(f"Error: Vent {vent_id} not found or not connected")
                        else:
                            print(f"Error: Invalid value format for type 3 packet: {value_str}")
                    except Exception as e:
                        print(f"Error processing vent position packet: {e}")
                
                elif pkt_type == 1:  # New vent setup request
                    print("Received vent setup request, starting setup...")
                    
                    # Setup a new vent cover and store its VentID
                    VentID = vent_system.add_vent_cover()
                    
                    try:
                        # Connect to BLE client using the async loop
                        future = asyncio.run_coroutine_threadsafe(connect_to_ble_client(value_str), loop)
                        client, device = future.result()  # Wait for connection
                        
                        if client:
                            # Store BLE connection
                            vent_system.get_vent_cover(VentID).set_ble_connection(client)
                            
                            # Start a thread to handle notifications and messages
                            ble_thread = threading.Thread(
                                target=ble_connection_thread, 
                                args=(client, VentID, self)
                            )
                            vent_system.get_vent_cover(VentID).set_ble_thread(ble_thread)
                            ble_thread.start()
                        
                    except KeyboardInterrupt:
                        logger.info("Script terminated by user")
                    except Exception as e:
                        logger.error(f"Unexpected error: {str(e)}")
                
                elif pkt_type == 2:
                    print("Received temperature mode request...")
                    
                    if '.' in value_str:
                        parts = value_str.split('.')
                        vent_id = int(parts[0])
                        temperature = parts[1]
                    
                    vent = vent_system.get_vent_cover(vent_id)
                    vent.user_forced = True
                    vent.desired_temp = float(temperature)
                    
                    
                        
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
        
        # Start the event loop in a separate thread
        loop_thread = threading.Thread(target=self.run_event_loop, daemon=True)
        loop_thread.start()
        
        # Start receive thread
        self.recv_thread = threading.Thread(target=self.receive_packets)
        self.recv_thread.start()
        
        # Main loop for testing
        try:
            while True:
                time.sleep(5)  # Sleep for 5 seconds between iterations
        except KeyboardInterrupt:
            print("\nShutting down...")
            self.stop()

    def run_event_loop(self):
        """Run the asyncio event loop in a separate thread"""
        asyncio.set_event_loop(loop)
        loop.run_forever()

    def stop(self):
        """Clean shutdown of the communicator"""
        print("Stopping communicator...")
        self.running = False
            # Properly disconnect all BLE connections
        print("Disconnecting BLE devices...")
        for vent in vent_system.vent_covers:
            if vent.is_connected():
                try:
                # Create a future to execute the async disconnect operation
                    future = asyncio.run_coroutine_threadsafe(
                    vent.get_ble_connection().disconnect(),
                    loop
                    )
                    # Wait for disconnect with timeout
                    future.result(timeout=3.0)
                    print(f"Disconnected vent {vent.vent_id} BLE connection")
                except Exception as e:
                    print(f"Error disconnecting vent {vent.vent_id}: {str(e)}")
        # Close the sockets
        try:
            self.recv_socket.close()
            self.send_socket.close()
        except Exception as e:
            print(f"Error closing sockets: {e}")
        
        # Wait for receive thread to finish (with timeout)
        print("Waiting for threads to finish...")
        self.recv_thread.join(timeout=2.0)  # Wait up to 2 seconds
        
        
        # Clean up BLE threads
        for vent in vent_system.vent_covers:
            if vent.ble_thread:
                vent.ble_thread.join(timeout=2.0)
        
        # Stop the event loop
        loop.call_soon_threadsafe(loop.stop)
        
        if self.recv_thread.is_alive():
            print("Warning: Thread did not terminate cleanly")
        else:
            print("Cleanup completed successfully")

if __name__ == '__main__':
    communicator = VentCommunicator()
    communicator.start()