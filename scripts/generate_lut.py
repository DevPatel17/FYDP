#!/usr/bin/env python3
"""
    To run:
        cd /path/to/this/file
        python3 generate_lut.py

    Generates a lookup table in your current working directory.
    Edit the LUT_NAME, LUT_FUNC, and LUT_SIZE variables to generate the LUT that you need.

    Note: the size of the LUT in bytes will be 4*LUT_SIZE
"""

import numpy as np

COL_SIZE = 8

LUT_NAME = "ADC_TO_TEMP_LUT"
LUT_SIZE = (1 << 12)  # 4096 values for 12-bit ADC

# arguments: x is the adc value ranging from 0 to 4095
def LUT_FUNC(x):
    # resistor value at 25°C in ohms
    r_nominal = 10000
    # resistor value used in voltage divider in ohms
    r1 = 10000
    # Beta coefficient from thermistor datasheet
    B = 3950
    # Room temperature in Kelvin
    room_t_kelvin = 298.15

    # Convert ADC value to voltage (0-3.3V range)
    v_out = x * (3.3 / 4095)  # Use 4095 as max to avoid out-of-bounds

    # Handle edge cases to avoid division by zero
    if abs(v_out - 3.3) < 1e-9:
        v_out = 3.299
    elif abs(v_out) < 1e-9:
        v_out = 0.001

    # Calculate thermistor resistance using voltage divider equation
    # For a pull-up configuration: R_thermistor = R1 * (Vcc/Vout - 1)
    r_thermistor = r1 * (3.3 / v_out - 1)
    
    # Handle unrealistic resistance values
    if r_thermistor <= 0:
        return 150.0
    
    # Apply Steinhart-Hart equation
    T_kelvin = 1 / ((1 / room_t_kelvin) + (1 / B) * np.log(r_thermistor / r_nominal))
    T_celsius = T_kelvin - 273.15
    
    # Limit to a reasonable temperature range to avoid outliers
    if T_celsius < -50:
        T_celsius = -50
    elif T_celsius > 150:
        T_celsius = 150
    
    return T_celsius

def main():
    # Create the lookup table
    lut = [LUT_FUNC(x) for x in range(LUT_SIZE)]

    # Write the lookup table to a C file
    with open(f"{LUT_NAME}.c", "w") as f:
        f.write(f"/**\n * {LUT_NAME} - ADC to Temperature (°C) lookup table\n")
        f.write(f" * Generated for a 12-bit ADC with a 10K thermistor (B=3950)\n */\n\n")
        f.write(f"const float {LUT_NAME}[{LUT_SIZE}] = {{\n    ")

        for i, lut_i in enumerate(lut):
            f.write(f"{lut_i:.2f}f")

            if i != (len(lut) - 1):
                f.write(", ")

            if ((i + 1) % COL_SIZE == 0) and (i != len(lut) - 1):
                f.write("\n    ")

        f.write("\n};\n")
    
    print(f"Generated lookup table '{LUT_NAME}.c' with {LUT_SIZE} entries.")
    
    # Also generate a header file
    with open(f"{LUT_NAME}.h", "w") as f:
        guard = f"{LUT_NAME}_H"
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")
        f.write(f"/**\n * {LUT_NAME} - ADC to Temperature (°C) lookup table\n")
        f.write(f" * For 12-bit ADC (0-4095) with a 10K thermistor (B=3950)\n */\n\n")
        f.write(f"extern const float {LUT_NAME}[{LUT_SIZE}];\n\n")
        f.write("#endif\n")
    
    print(f"Generated header file '{LUT_NAME}.h'")

if __name__ == "__main__":
    main()