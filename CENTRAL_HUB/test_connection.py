#!/usr/bin/env python3
import socket
import struct
import time
import threading
import asyncio
from bleak import BleakScanner, BleakClient
import logging
from collections import deque
import sys

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Temperature control method configuration
# Set to 'PID' or 'HYSTERESIS' to choose the temperature control method
TEMPERATURE_CONTROL_METHOD = 'HYSTERESIS'  # Options: 'PID', 'HYSTERESIS'

# Hysteresis control configuration
HYSTERESIS_THRESHOLD_HIGH = 1.0  # Degrees above desired temperature to close vent
HYSTERESIS_THRESHOLD_LOW = 0.5   # Degrees below desired temperature to open vent
HYSTERESIS_MIN_POSITION = 0      # Minimum vent position (closed)
HYSTERESIS_MAX_POSITION = 100    # Maximum vent position (open)

# The device names we're looking for (same as in your ESP32 code)
DEVICE_NAMES = ["BLE-Server1", "BLE-Server2"]

# The UUIDs from your ESP32 code
READ_UUID = "00000180-0000-1000-8000-00805f9b34fb"    # Service UUID
WRITE_UUID = "0000dead-0000-1000-8000-00805f9b34fb"   # Write characteristic UUID
READ_CHAR_UUID = "0000fef4-0000-1000-8000-00805f9b34fb"  # Read characteristic UUID

# Create a global event loop to be shared across threads
loop = asyncio.new_event_loop()

# Connection tracking
connected_devices = {}
pending_connections = set()

async def find_device(device_name, timeout=30):
    """Scan for the ESP32 device with timeout"""
    logger.info(f"Scanning for {device_name}...")
    
    start_time = time.time()
    while time.time() - start_time < timeout:
        devices = await BleakScanner.discover()
        for device in devices:
            # Check device name and also that we're not already trying to connect to it
            if device.name == device_name and device.address not in pending_connections:
                logger.info(f"Found {device.name}: {device.address}")
                return device
        logger.info(f"Device {device_name} not found, scanning again...")
        await asyncio.sleep(2)
    
    logger.error(f"Timeout: Could not find device {device_name} within {timeout} seconds")
    return None

async def connect_to_ble_client(device_name, retries=3, vent_id=None):
    """
    Connects to the BLE device and returns the client object
    with retry mechanism
    
    Args:
        device_name (str): The name of the BLE device to connect to
        retries (int): Number of connection retries
        vent_id (int, optional): The vent ID if this is a reconnection attempt
        
    Returns:
        tuple: (client, device) - The BLE client and device objects if successful, (None, None) otherwise
    """
    device = None
    client = None
    
    # Log differently based on whether this is a new connection or reconnection
    if vent_id:
        logger.info(f"Attempting to reconnect to vent {vent_id} with device name {device_name}")
    else:
        logger.info(f"Attempting to connect to new device {device_name}")
    
    for attempt in range(retries):
        try:
            # Mark this device as pending connection
            if device:
                pending_connections.add(device.address)
                
            device = await find_device(device_name)
            if not device:
                logger.error(f"Device {device_name} not found after scanning")
                if attempt < retries - 1:
                    logger.info(f"Retrying scan for {device_name}... (Attempt {attempt+2}/{retries})")
                    await asyncio.sleep(2)
                continue
            
            # Create a disconnection callback that captures the vent_id for reconnection
            def disconnection_callback(client):
                nonlocal vent_id
                logger.warning(f"Device {client.address} was disconnected!")
                
                # If we have a vent_id, mark it for reconnection
                if vent_id is not None:
                    vent = vent_system.get_vent_cover(vent_id)
                    if vent:
                        # Ensure bleName is kept
                        logger.info(f"Marking vent {vent_id} as disconnected, bleName={vent.bleName}")
                        vent.disconnect()  # This will set last_disconnect_time
                
            # Use connection options for better stability
            client = BleakClient(
                device.address,
                timeout=20.0,  # Increase connection timeout
                disconnected_callback=disconnection_callback
            )
            
            logger.info(f"Attempting to connect to {device_name} at {device.address}...")
            connected = await client.connect()
            
            if connected:
                logger.info(f"Successfully connected to {device_name}")
                # Add to connected devices
                connected_devices[device.address] = client
                
                # Update the vent with the device name for future reconnections
                if vent_id is not None:
                    vent = vent_system.get_vent_cover(vent_id)
                    if vent:
                        vent.bleName = device_name
                        vent.reconnect_attempts = 0  # Reset reconnection attempts
                
                return client, device
            else:
                logger.error(f"Connection failed to {device_name}")
                if attempt < retries - 1:
                    logger.info(f"Retrying connection to {device_name}... (Attempt {attempt+2}/{retries})")
                await asyncio.sleep(2)
                
        except Exception as e:
            logger.error(f"Error connecting to {device_name}: {str(e)}")
            if attempt < retries - 1:
                logger.info(f"Retrying after error... (Attempt {attempt+2}/{retries})")
            # Clean up the failed client
            if client and client.is_connected:
                try:
                    await client.disconnect()
                except:
                    pass
            await asyncio.sleep(2)
        finally:
            # Remove from pending if it was added
            if device and device.address in pending_connections:
                pending_connections.remove(device.address)
    
    # If this was a reconnection attempt, increment the counter
    if vent_id is not None:
        vent = vent_system.get_vent_cover(vent_id)
        if vent:
            vent.reconnect_attempts += 1
            logger.warning(f"Failed reconnection attempt {vent.reconnect_attempts}/{vent.max_reconnect_attempts} for vent {vent_id}")
    
    logger.error(f"Failed to connect to {device_name} after {retries} attempts")
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
            try:
                message = data.decode()
                logger.info(f"Received notification from {sender}: {message}")
                
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
                            
                            # Get the vent instance
                            vent = vent_system.get_vent_cover(vent_id)
                            if not vent:
                                logger.error(f"Could not find vent with ID: {vent_id}")
                                return
                                
                            temp_change = abs(float(temp) - vent.temperature) > 0.1
                            
                            if vent.user_forced == True and temp_change == True:
                                # Choose temperature control method based on global configuration
                                if TEMPERATURE_CONTROL_METHOD == 'PID':
                                    # Use existing PID controller
                                    new_pos = int(vent.PIDController.compute(temp, vent.desired_temp))
                                    logger.info(f"PID Controller - New vent position: {new_pos}")
                                    send_vent_position(vent, new_pos)
                                elif TEMPERATURE_CONTROL_METHOD == 'HYSTERESIS':
                                    # Use hysteresis controller
                                    new_pos = compute_hysteresis_position(temp, vent.desired_temp, vent.vent_cover_status)
                                    if new_pos != vent.pos:
                                        send_vent_position(vent, new_pos)
                                        vent.pos = new_pos
                                        logger.info(f"Hysteresis Controller - New vent position: {new_pos}")
                            vent.temperature = temp
                        elif "ID:" in message and "motor:" in message:
                            parts = message.split(',')
                            vent_id = int(parts[0].split(':')[1].strip())
                            motor_pos = float(parts[1].split(':')[1].strip())
                            
                            logger.info(f"Parsed - Vent ID: {vent_id}, Motor position: {motor_pos}")
                            
                            # Send type 3 packet with combined vent ID and motor position
                            value_str = f"{vent_id}.motor{motor_pos}"
                            phone_connection.send_packet(3, value_str) 
                            
                            vent = vent_system.get_vent_cover(vent_id)
                            if vent:
                                vent.user_forced = False
                    except Exception as e:
                        logger.error(f"Error parsing message: {e}")
            except Exception as e:
                logger.error(f"Error in notification handler: {e}")

        # Enable notifications for the characteristic
        try:
            await client.start_notify(READ_CHAR_UUID, notification_handler)
            logger.info("Notifications enabled successfully")

            # Write initial connection message
            message = f"Connected, Vent ID: {VentID}".encode()
            await client.write_gatt_char(WRITE_UUID, message)
            logger.info(f"Sent message: {message.decode()}")
            
            # Keep the connection alive with a heartbeat
            while True:
                # Regularly check if still connected
                if not client.is_connected:
                    logger.warning(f"BLE client disconnected for Vent ID: {VentID}")
                    break
                    
                await asyncio.sleep(5)
        except Exception as e:
            logger.error(f"Error during BLE operation: {e}")
            raise

    except Exception as e:
        logger.error(f"Error in BLE connection: {str(e)}")
    finally:
        try:
            if client and client.is_connected:
                logger.info(f"Disconnecting client for Vent ID: {VentID}")
                await client.disconnect()
        except Exception as e:
            logger.error(f"Error disconnecting: {e}")
        
        # Update vent to show disconnected status
        vent = vent_system.get_vent_cover(VentID)
        if vent:
            vent.disconnect()
            
        # Remove from connected devices
        address = next((addr for addr, c in connected_devices.items() if c == client), None)
        if address:
            connected_devices.pop(address, None)

def compute_hysteresis_position(current_temp, desired_temp, current_position):
    """
    Compute vent position using hysteresis control.
    
    Args:
        current_temp (float): Current temperature reading
        desired_temp (float): Desired target temperature
        current_position (int): Current vent position
        
    Returns:
        int: New vent position (0-100)
    """
    # For cooling mode: open vent when too warm, close when cool enough
    if current_temp > (desired_temp + HYSTERESIS_THRESHOLD_HIGH):
        # Temperature is too high, open vent to allow cooling
        return HYSTERESIS_MAX_POSITION
    elif current_temp < (desired_temp - HYSTERESIS_THRESHOLD_LOW):
        # Temperature is below target, close vent to reduce cooling
        return HYSTERESIS_MIN_POSITION
    else:
        # Within acceptable range, maintain current position
        return current_position

async def ble_connection_wrapper(client, VentID, phone_connection):
    """
    Async wrapper function to handle BLE connection with error handling and retry
    """
    retry_count = 0
    max_retries = 3
    
    while retry_count < max_retries:
        try:
            await run_ble_connection(client, VentID, phone_connection)
            break  # If run_ble_connection completes normally, exit the loop
        except Exception as e:
            retry_count += 1
            logger.error(f"BLE connection error (attempt {retry_count}/{max_retries}): {str(e)}")
            
            if retry_count < max_retries:
                logger.info(f"Retrying BLE connection in 5 seconds...")
                await asyncio.sleep(5)
            else:
                logger.error(f"Maximum retry attempts reached. Giving up.")
                
                # Update vent to show disconnected status
                vent = vent_system.get_vent_cover(VentID)
                if vent:
                    vent.disconnect()
                break

def ble_connection_thread(client, VentID, phone_connection):
    """
    Thread function to handle BLE connection
    This function sets up a new event loop for the thread
    """
    # Set the event loop for this thread
    asyncio.set_event_loop(loop)
    
    # Submit the task to the event loop
    future = asyncio.run_coroutine_threadsafe(
        ble_connection_wrapper(client, VentID, phone_connection), 
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
    
    def __init__(self, temperature=0.0, vent_cover_status=0, user_forced=False, vent_id=None, bleName=None):
        self.temperature = float(temperature)
        self.desired_temp = float(temperature)
        self.vent_cover_status = int(vent_cover_status)
        self.user_forced = bool(user_forced)
        self.ble_connection = None  # Default to no connection
        self.ble_thread = None
        self.PIDController = VentPIDController()
        self.pos = -1
        self.last_command_time = 0  # Track when commands were sent
        self.command_queue = deque(maxlen=5)  # Queue for commands
        self.bleName = bleName  # Store the BLE device name for reconnection
        self.last_disconnect_time = 0  # Track when disconnection happened
        self.reconnect_attempts = 0  # Track number of reconnection attempts
        self.max_reconnect_attempts = 5  # Maximum number of reconnection attempts
        
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
        return self.ble_connection is not None and self.ble_connection.is_connected
    
    def queue_command(self, command):
        """Add a command to the queue with rate limiting"""
        self.command_queue.append(command)
        
    def get_next_command(self):
        """Get the next command from the queue if available"""
        if self.command_queue:
            return self.command_queue.popleft()
        return None
    
    def disconnect(self):
        """Handle disconnection of BLE connection"""
        self.ble_connection = None
        self.last_disconnect_time = time.time()
        logger.info(f"Vent {self.vent_id} disconnected, will attempt reconnection if bleName is available")
    
    def __str__(self):
        connected_status = "Connected" if self.is_connected() else "Not Connected"
        return f"VentCover(id={self.vent_id}, temperature={self.temperature}, status={self.vent_cover_status}, user_forced={self.user_forced}, BLE: {connected_status})"
    
class VentCoverSystem:
    def __init__(self):
        self.vent_covers = []
        self._lock = threading.RLock()  # Add lock for thread safety
    
    def add_vent_cover(self, temperature=0.0, vent_cover_status=0, user_forced=False):
        """Create and add a new vent cover to the system"""
        with self._lock:
            new_vent = VentCover(temperature, vent_cover_status, user_forced)
            self.vent_covers.append(new_vent)
            return new_vent.vent_id
    
    def remove_vent_cover(self, vent_id):
        """Remove a vent cover by its ID"""
        with self._lock:
            self.vent_covers = [vent for vent in self.vent_covers if vent.vent_id != vent_id]
    
    def get_vent_cover(self, vent_id):
        """Get a vent cover by its ID"""
        with self._lock:
            for vent in self.vent_covers:
                if vent.vent_id == vent_id:
                    return vent
            return None
    
    def get_all_vent_covers(self):
        """Return all vent covers"""
        with self._lock:
            return self.vent_covers.copy()
    
    def update_vent_cover(self, vent_id, temperature=None, vent_cover_status=None, user_forced=None):
        """Update a vent cover's properties"""
        with self._lock:
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
        with self._lock:
            return "\n".join(str(vent) for vent in self.vent_covers)


vent_system = VentCoverSystem()

def send_vent_position(vent, position):
    """Send a position command to a vent over BLE with rate limiting"""
    if not vent.is_connected():
        logger.warning(f"Cannot send position to vent {vent.vent_id}: not connected")
        return
    
    # Rate limit commands
    current_time = time.time()
    if current_time - vent.last_command_time < 0.5:  # Minimum 500ms between commands
        # Queue the command instead
        vent.queue_command(str(position))
        logger.debug(f"Rate limited command to vent {vent.vent_id}, queued position: {position}")
        return
        
    vent.last_command_time = current_time
        
    # Create a future to execute the async BLE write operation
    async def send_position_command(client, position):
        try:
            # Make sure position is a string
            pos_str = str(position)
            
            # Encode the position command
            message = pos_str.encode()
            
            # Verify client is still connected
            if not client.is_connected:
                logger.error(f"Client for vent {vent.vent_id} is no longer connected")
                return False
                
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
        result = future.result(timeout=3.0)
        if result:
            logger.info(f"Successfully sent position {position} to vent {vent.vent_id}")
        else:
            logger.warning(f"Failed to send position to vent {vent.vent_id}")
    except Exception as e:
        logger.error(f"Error in send_vent_position: {str(e)}")

# Command processor thread to handle queued commands
def command_processor_thread():
    """Process queued commands with rate limiting"""
    while True:
        try:
            # Check all vents for queued commands
            for vent in vent_system.get_all_vent_covers():
                if vent.is_connected():
                    # Get next command if available and not rate limited
                    command = vent.get_next_command()
                    if command and time.time() - vent.last_command_time >= 0.5:
                        send_vent_position(vent, command)
                        vent.last_command_time = time.time()
                        
            # Sleep a bit to prevent CPU hogging
            time.sleep(0.1)
        except Exception as e:
            logger.error(f"Error in command processor: {e}")
            time.sleep(1)  # Longer sleep on error

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
        
        # Command processor thread
        self.cmd_processor = threading.Thread(target=command_processor_thread, daemon=True)
        self.cmd_processor.start()
    
    def send_packet(self, pkt_type, value):
        """Send a packet to the phone"""
        if self.phone_address is None:
            logger.warning("No phone address known yet")
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
            logger.info(f'Sent packet - Type: {pkt_type}, Value: {value}')
        except Exception as e:
            logger.error(f'Error sending packet: {e}')

    def receive_packets(self):
        while self.running:
            try:
                data, address = self.recv_socket.recvfrom(1024)
                
                # Store the phone's address for sending responses
                self.phone_address = address
                
                # Extract packet type and value
                if len(data) < 4:
                    logger.warning(f"Received packet too short: {len(data)} bytes")
                    continue
                    
                pkt_type = struct.unpack('<I', data[0:4])[0]
                
                # Extract the string value after the packet type
                value_str = ""
                if len(data) > 4:
                    try:
                        # Try to decode the remaining bytes as a UTF-8 string
                        value_str = data[4:].decode('utf-8')
                    except Exception as e:
                        logger.error(f"Error decoding string value: {e}")
                
                logger.info(f'Received from {address}:')
                logger.info(f'  Type: {pkt_type}')
                logger.info(f'  Value: {value_str}')
                
                if pkt_type == 3:  # Vent position control request
                    try:
                        # Handle vent position control
                        if '.' in value_str:
                            parts = value_str.split('.')
                            vent_id = int(parts[0])
                            vent_position = parts[1]
                            
                            logger.info(f"Vent control request - Vent ID: {vent_id}, Position: {vent_position}")
                            
                            # Get the vent cover and check if it has a BLE connection
                            vent = vent_system.get_vent_cover(vent_id)
                            
                            if vent:
                                vent.user_forced = False
                                if vent.is_connected():
                                    # Send vent position command to the vent via BLE
                                    send_vent_position(vent, vent_position)
                                else:
                                    logger.warning(f"Vent {vent_id} is not connected")
                            else:
                                logger.error(f"Error: Vent {vent_id} not found")
                        else:
                            logger.error(f"Error: Invalid value format for type 3 packet: {value_str}")
                    except Exception as e:
                        logger.error(f"Error processing vent position packet: {e}")
                
                elif pkt_type == 1:  # New vent setup request
                    logger.info("Received vent setup request, starting setup...")
                    
                    # Setup a new vent cover and store its VentID
                    VentID = vent_system.add_vent_cover()
                    
                    try:
                        # Connect to BLE client using the async loop
                        future = asyncio.run_coroutine_threadsafe(connect_to_ble_client(value_str), loop)
                        try:
                            client, device = future.result(timeout=30)  # Wait for connection with timeout
                            
                            if client:
                                # Store BLE connection
                                vent = vent_system.get_vent_cover(VentID)
                                if vent:
                                    vent.set_ble_connection(client)
                                    vent.bleName = value_str 
                                    # Start a thread to handle notifications and messages
                                    ble_thread = threading.Thread(
                                        target=ble_connection_thread, 
                                        args=(client, VentID, self),
                                        daemon=True
                                    )
                                    vent.set_ble_thread(ble_thread)
                                    ble_thread.start()
                                else:
                                    logger.error(f"Could not find vent with ID {VentID}")
                            else:
                                logger.error("Failed to connect to BLE device")
                        except concurrent.futures.TimeoutError:
                            logger.error("Connection attempt timed out")
                        
                    except KeyboardInterrupt:
                        logger.info("Script terminated by user")
                    except Exception as e:
                        logger.error(f"Unexpected error during vent setup: {str(e)}")
                
                elif pkt_type == 2:  # Temperature mode request
                    logger.info("Received temperature mode request...")
                    
                    try:
                        if '.' in value_str:
                            parts = value_str.split('.')
                            vent_id = int(parts[0])
                            temperature = parts[1]
                        
                            vent = vent_system.get_vent_cover(vent_id)

                            if vent is not None: 
                                vent.user_forced = True
                                vent.desired_temp = float(temperature)
                                logger.info(f"Set desired temperature for vent {vent_id} to {temperature}")
                            else:
                                logger.error(f"Could not find vent with ID {vent_id}")
                        else:
                            logger.error(f"Invalid temperature mode request format: {value_str}")
                    except Exception as e:
                        logger.error(f"Error processing temperature mode request: {e}")
                        
            except socket.timeout:
                # This is normal, just continue the loop
                continue
            except Exception as e:
                if self.running:  # Only print errors if we're still meant to be running
                    logger.error(f'Error receiving packet: {e}')

    def start(self):
        """Start the communication threads"""
        print('Starting UDP server...')
        print(f'Temperature control method: {TEMPERATURE_CONTROL_METHOD}')
        
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
        self.recv_thread = threading.Thread(target=self.receive_packets, daemon=True)
        self.recv_thread.start()
        
        # Connection health monitoring thread
        monitor_thread = threading.Thread(target=self.monitor_connections, daemon=True)
        monitor_thread.start()
        
        # Main loop for testing
        try:
            while True:
                time.sleep(5)  # Sleep for 5 seconds between iterations
                self.print_status()  # Print status update periodically
        except KeyboardInterrupt:
            print("\nShutting down...")
            self.stop()

    def monitor_connections(self):
        """Monitor and maintain BLE connections with reconnection attempts"""
        while self.running:
            try:
                # For each vent, check if it's still connected
                for vent in vent_system.get_all_vent_covers():
                    # Log the status including the bleName
                    logger.debug(f"Vent {vent.vent_id}: connected={vent.is_connected()}, bleName={vent.bleName}, attempts={vent.reconnect_attempts}/{vent.max_reconnect_attempts}")
                    # Check if connection is lost
                    if vent.ble_connection and not vent.is_connected():
                        logger.warning(f"Detected disconnected vent {vent.vent_id}, updating status")
                        vent.disconnect()  # Update internal status
                    
                    # Check if we need to attempt reconnection
                    if not vent.is_connected() and vent.bleName and vent.reconnect_attempts < vent.max_reconnect_attempts:
                        # Check if enough time has passed since last disconnect or reconnect attempt
                        time_since_disconnect = time.time() - vent.last_disconnect_time
                        
                        # Use exponential backoff for reconnection attempts
                        # Wait longer between each attempt: 5s, 10s, 20s, 40s, 80s
                        backoff_time = 5 * (2 ** vent.reconnect_attempts)
                        
                        if time_since_disconnect >= backoff_time:
                            logger.info(f"Attempting to reconnect to vent {vent.vent_id} with name {vent.bleName} (attempt {vent.reconnect_attempts + 1}/{vent.max_reconnect_attempts})")
                            
                            # Attempt reconnection
                            future = asyncio.run_coroutine_threadsafe(
                                connect_to_ble_client(vent.bleName, retries=1, vent_id=vent.vent_id),
                                loop
                            )
                            
                            try:
                                # Wait for connection with timeout
                                client, device = future.result(timeout=15)
                                
                                if client:
                                    # Update the vent's connection
                                    vent.set_ble_connection(client)
                                    
                                    # Start a new notification thread if needed
                                    if not vent.ble_thread or not vent.ble_thread.is_alive():
                                        ble_thread = threading.Thread(
                                            target=ble_connection_thread, 
                                            args=(client, vent.vent_id, self),
                                            daemon=True
                                        )
                                        vent.set_ble_thread(ble_thread)
                                        ble_thread.start()
                                        
                                    logger.info(f"Successfully reconnected to vent {vent.vent_id}")
                                else:
                                    # Update the last disconnect time to trigger next attempt after backoff
                                    vent.last_disconnect_time = time.time()
                                    logger.warning(f"Failed to reconnect to vent {vent.vent_id}")
                            except concurrent.futures.TimeoutError:
                                logger.error(f"Reconnection attempt to vent {vent.vent_id} timed out")
                                vent.last_disconnect_time = time.time()  # Update for backoff
                            except Exception as e:
                                logger.error(f"Error during reconnection attempt for vent {vent.vent_id}: {str(e)}")
                                vent.last_disconnect_time = time.time()  # Update for backoff
                
                # Sleep for a bit before checking again
                time.sleep(5)
            except Exception as e:
                logger.error(f"Error in connection monitor: {e}")
                time.sleep(5)

    def print_status(self):
        """Print the current status of the system"""
        logger.info("--- System Status ---")
        logger.info(f"Connected devices: {len(connected_devices)}")
        
        for vent in vent_system.get_all_vent_covers():
            connection_status = "Connected" if vent.is_connected() else "Disconnected"
            logger.info(f"Vent {vent.vent_id}: Temp={vent.temperature}°C, Target={vent.desired_temp}°C, {connection_status}")
        
        logger.info("-------------------")

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
        for vent in vent_system.get_all_vent_covers():
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
        # self.recv_thread.join(timeout=2.0)  # Wait up to 2 seconds
        
        # Clean up BLE threads
        # for vent in vent_system.get_all_vent_covers():
        #     if vent.ble_thread:
        #         vent.ble_thread.join(timeout=2.0)
        
        # Stop the event loop
        try:
            loop.call_soon_threadsafe(loop.stop)
        except Exception as e:
            print(f"Error stopping event loop: {e}")
        
        print("Cleanup completed.")
        sys.exit(0)

if __name__ == '__main__':
    # Import required module here to avoid circular imports
    import concurrent.futures
    
    try:
        communicator = VentCommunicator()
        communicator.start()
    except Exception as e:
        logger.critical(f"Critical error in main: {e}")
        sys.exit(1)