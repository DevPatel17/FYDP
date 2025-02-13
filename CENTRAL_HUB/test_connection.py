import os
from bluepy.btle import Peripheral, DefaultDelegate, BTLEException
import time
import subprocess

class BLEPeripheral(Peripheral):
    def __init__(self):
        Peripheral.__init__(self)
        
def setup_ble_advertisement():
    # Stop bluetooth service to reconfigure
    os.system("sudo systemctl stop bluetooth")
    time.sleep(1)
    
    # Reset and configure bluetooth
    os.system("sudo btmgmt power off")
    os.system("sudo btmgmt power on")
    os.system("sudo btmgmt le on")
    
    # Set up advertisement data using hcitool
    os.system("sudo hcitool -i hci0 cmd 0x08 0x0008 1E 02 01 06 1A FF 4C 00 02 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00")
    os.system("sudo hcitool -i hci0 cmd 0x08 0x0006 00 08 00 00 00 00 00 00 00 00 00 00 00 07")
    os.system("sudo hcitool -i hci0 cmd 0x08 0x000a 01")
    
    # Start bluetooth service
    os.system("sudo systemctl start bluetooth")
    time.sleep(2)

def advertise_and_listen():
    try:
        print("Setting up BLE advertisement...")
        setup_ble_advertisement()
        
        print("Starting advertisement as 'Raspberry Pi'...")
        # Set device name
        os.system("sudo hciconfig hci0 name 'Raspberry Pi'")
        
        # Enable advertising
        os.system("sudo hciconfig hci0 leadv 0")  # Using non-connectable advertising for visibility
        
        print("Device is now advertising. Should be visible to iOS devices...")
        
        while True:
            # Check for connections
            result = subprocess.run(['hcitool', 'con'], capture_output=True, text=True)
            if ">" in result.stdout:
                print("Device connected!")
                print("Waiting for incoming packets...")
            time.sleep(1)
            
    except BTLEException as e:
        print(f"Bluetooth error: {e}")
    except KeyboardInterrupt:
        print("\nStopping advertisement...")
        os.system("sudo hciconfig hci0 noleadv")
    finally:
        os.system("sudo hciconfig hci0 noleadv")

if __name__ == "__main__":
    advertise_and_listen()