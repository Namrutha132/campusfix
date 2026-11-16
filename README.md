# CampusFix — College Complaint and Maintenance Management System

## Technologies
- Frontend: HTML, CSS and JavaScript
- Backend: C (Winsock HTTP server on Windows)
- Storage: text files using C file handling

## Features
- Student registration and login
- Submit complaints under Electrical, Water, Cleaning, Classroom or Other
- Track a student's submitted complaints
- Admin login and complaint overview
- Admin can change complaint status to Pending, In Progress or Resolved
- Complaint totals by status

## Requirements
Windows with GCC (MinGW-w64 recommended).

## Run the project
1. Extract the ZIP file.
2. Open the extracted `CampusFix` folder in VS Code.
3. Make sure the `data` folder exists (it already contains empty data files).
4. Open VS Code Terminal: Terminal → New Terminal.
5. Ensure the terminal is in the main `CampusFix` folder, not inside `backend` or `frontend`.
6. Compile:

   `gcc backend/server.c -o campusfix.exe -lws2_32`

7. Run:

   `.\campusfix.exe`

8. Open this in your browser:

   `http://localhost:8080`

9. Keep the terminal running while using the website. Press Ctrl+C in the terminal to stop the server.

## Admin demo login
- Username: `admin`
- Password: `admin123`

## Important academic-project notes
- The C program handles HTTP requests, registration/login checks, complaint storage, listing and status updates.
- HTML and CSS build the interface; JavaScript sends requests to the C server.
- Student and complaint records are stored in `data/students.txt` and `data/complaints.txt`.
- The backend uses a fixed-size complaint array for this classroom demonstration.
- This is a local academic prototype, not a production-ready secure service. Passwords are stored as plain text, the admin credentials are fixed for demonstration, and there is no production authentication/session security. Use dummy passwords only.
- The simple request parser is intended for the forms in this project, not arbitrary internet traffic.
