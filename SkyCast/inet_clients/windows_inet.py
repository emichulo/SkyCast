import socket
import time
import os

SERVER_ADDRESS = "127.0.0.1"
SERVER_PORT = 8080

def display_menu():
    print("\nMenu:")
    print("1. Get current weather")
    print("2. Get air quality")
    print("3. Get historic weather")
    print("4. Get future weather")
    print("5. Exit")
    print("===========================")

def clear_console():
    os.system('cls' if os.name == 'nt' else 'clear')

def send_request(sock, request):
    sock.sendall(request.encode())

def send_location(sock, location):
    sock.sendall(location.encode())

def send_date(sock, date):
    sock.sendall(date.encode())

def read_response(sock):
    response = sock.recv(1024).decode()
    print("Response from server:\n" + response)

def is_date_valid_future(date):
    current_time = time.time()
    current_date = time.localtime(current_time)

    try:
        input_date = time.strptime(date, "%Y-%m-%d")
    except ValueError:
        print("Error: Invalid date format. Please use the format YYYY-MM-DD.")
        return False

    input_time = time.mktime(input_date)
    seconds_difference = input_time - current_time
    days_difference = seconds_difference / (60 * 60 * 24)

    if days_difference < 14 or days_difference > 300:
        print("Error: Invalid date. The provided date should be between 14 and 300 days from today.")
        return False

    return True

def is_date_valid_history(date):
    current_time = time.time()
    current_date = time.localtime(current_time)

    try:
        input_date = time.strptime(date, "%Y-%m-%d")
    except ValueError:
        print("Error: Invalid date format. Please use the format YYYY-MM-DD.")
        return False

    diff = current_time - time.mktime(input_date)
    days_difference = diff / (60 * 60 * 24)

    if days_difference > 365:
        print("Error: Invalid date. The provided date is older than 365 days.")
        return False

    if time.mktime(input_date) > current_time:
        print("Error: Invalid date. The provided date is in the future.")
        return False

    return True

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sock.connect((SERVER_ADDRESS, SERVER_PORT))
    except ConnectionRefusedError:
        print("Error: could not connect to server.")
        return

    clear_console()
    while True:
        display_menu()
        choice = input("Enter your choice: ")

        if choice == "1":
            send_request(sock, "get_current_weather")
            clear_console()

            location = input("Enter location: ")
            send_location(sock, location)

            read_response(sock)
        elif choice == "2":
            send_request(sock, "get_air_quality")
            clear_console()

            location = input("Enter location: ")
            send_location(sock, location)

            read_response(sock)
        elif choice == "3":
            send_request(sock, "get_historic_weather")
            clear_console()

            location = input("Enter location: ")
            send_location(sock, location)

            date = input("Enter date: ")
            send_date(sock, date)

            if not is_date_valid_history(date):
                continue

            read_response(sock)
        elif choice == "4":
            send_request(sock, "get_future_weather")
            clear_console()

            location = input("Enter location: ")
            send_location(sock, location)

            date = input("Enter date: ")
            send_date(sock, date)

            if not is_date_valid_future(date):
                continue

            read_response(sock)
        elif choice == "5":
            print("Exiting...")
            send_request(sock, "exit")
            clear_console()
            break
        else:
            print("Invalid choice. Please try again.")

    sock.close()

if __name__ == "__main__":
    main()
