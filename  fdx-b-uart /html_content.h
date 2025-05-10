#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Arduino Nano ESP32 Task Scheduler</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .card {
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            padding: 20px;
            margin-bottom: 20px;
        }
        .task-form {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .full-width {
            grid-column: span 2;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        input, select {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
            box-sizing: border-box;
        }
        button {
            background-color: #007BFF;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        button:hover {
            background-color: #0056b3;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }
        th, td {
            padding: 10px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background-color: #f2f2f2;
        }
        .task-actions {
            display: flex;
            gap: 5px;
        }
        .edit-btn {
            background-color: #FFC107;
        }
        .delete-btn {
            background-color: #DC3545;
        }
        .status {
            padding: 10px;
            margin-bottom: 15px;
            border-radius: 4px;
            display: none;
        }
        .success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .time-section {
            display: grid;
            grid-template-columns: repeat(5, 1fr);
            gap: 10px;
        }
    </style>
</head>
<body>
    <div class="card">
        <h1>Arduino Nano ESP32 Task Scheduler</h1>
        <div id="status" class="status"></div>
        
        <div class="card">
            <h2>Add/Edit Task</h2>
            <form id="taskForm" class="task-form">
                <div class="full-width">
                    <label for="taskName">Task Name:</label>
                    <input type="text" id="taskName" required>
                </div>
                
                <div class="full-width">
                    <h3>Schedule:</h3>
                    <div class="time-section">
                        <div>
                            <label for="minute">Minute:</label>
                            <input type="number" id="minute" min="-1" max="59" value="-1">
                            <small>(-1 for any)</small>
                        </div>
                        <div>
                            <label for="hour">Hour:</label>
                            <input type="number" id="hour" min="-1" max="23" value="-1">
                            <small>(-1 for any)</small>
                        </div>
                        <div>
                            <label for="day">Day:</label>
                            <input type="number" id="day" min="-1" max="31" value="-1">
                            <small>(-1 for any)</small>
                        </div>
                        <div>
                            <label for="month">Month:</label>
                            <input type="number" id="month" min="-1" max="12" value="-1">
                            <small>(-1 for any)</small>
                        </div>
                        <div>
                            <label for="dayOfWeek">Day of Week:</label>
                            <input type="number" id="dayOfWeek" min="-1" max="6" value="-1">
                            <small>(-1 for any, 0=Sun)</small>
                        </div>
                    </div>
                </div>
                
                <div class="full-width">
                    <button type="submit" id="submitBtn">Add Task</button>
                    <button type="button" id="cancelBtn" style="display: none;">Cancel Edit</button>
                </div>
            </form>
        </div>
        
        <div class="card">
            <h2>Current Tasks</h2>
            <table id="tasksTable">
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Minute</th>
                        <th>Hour</th>
                        <th>Day</th>
                        <th>Month</th>
                        <th>Day of Week</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody></tbody>
            </table>
        </div>
        
        <div class="card">
            <h2>Quick Presets</h2>
            <button onclick="addPreset('Every Minute', -1, -1, -1, -1, -1)">Every Minute</button>
            <button onclick="addPreset('Morning Feed', 0, 7, -1, -1, -1)">7:00 AM Daily</button>
            <button onclick="addPreset('Noon Feed', 30, 12, -1, -1, -1)">12:30 PM Daily</button>
            <button onclick="addPreset('Evening Feed', 0, 19, -1, -1, -1)">7:00 PM Daily</button>
        </div>
    </div>

    <script>
        // Task management
        let tasks = [];
        let editingIndex = -1;
        
        // DOM elements
        const form = document.getElementById('taskForm');
        const tasksTable = document.getElementById('tasksTable').getElementsByTagName('tbody')[0];
        const submitBtn = document.getElementById('submitBtn');
        const cancelBtn = document.getElementById('cancelBtn');
        const statusDiv = document.getElementById('status');
        
        // Initialize with existing tasks when page loads
        window.addEventListener('load', async () => {
            try {
                const response = await fetch('/get-tasks');
                if (response.ok) {
                    tasks = await response.json();
                    renderTasks();
                } else {
                    showStatus('Could not load tasks', false);
                }
            } catch (error) {
                showStatus('Error connecting to ESP32', false);
                
                // For testing: initialize with sample task if fetch fails
                tasks = [
                    { name: "Every Minute", minute: -1, hour: -1, dayOfMonth: -1, month: -1, dayOfWeek: -1 }
                ];
                renderTasks();
            }
        });
        
        // Form submission
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const task = {
                name: document.getElementById('taskName').value,
                minute: parseInt(document.getElementById('minute').value),
                hour: parseInt(document.getElementById('hour').value),
                dayOfMonth: parseInt(document.getElementById('day').value),
                month: parseInt(document.getElementById('month').value),
                dayOfWeek: parseInt(document.getElementById('dayOfWeek').value)
            };
            
            if (editingIndex === -1) {
                // Add new task
                tasks.push(task);
            } else {
                // Update existing task
                tasks[editingIndex] = task;
                editingIndex = -1;
                submitBtn.textContent = 'Add Task';
                cancelBtn.style.display = 'none';
            }
            
            try {
                await saveTasks();
                form.reset();
                showStatus('Task saved successfully!', true);
            } catch (error) {
                showStatus('Error saving task', false);
            }
            
            renderTasks();
        });
        
        // Cancel editing
        cancelBtn.addEventListener('click', () => {
            editingIndex = -1;
            form.reset();
            submitBtn.textContent = 'Add Task';
            cancelBtn.style.display = 'none';
        });
        
        // Add preset task
        function addPreset(name, minute, hour, day, month, dayOfWeek) {
            document.getElementById('taskName').value = name;
            document.getElementById('minute').value = minute;
            document.getElementById('hour').value = hour;
            document.getElementById('day').value = day;
            document.getElementById('month').value = month;
            document.getElementById('dayOfWeek').value = dayOfWeek;
        }
        
        // Edit task
        function editTask(index) {
            const task = tasks[index];
            document.getElementById('taskName').value = task.name;
            document.getElementById('minute').value = task.minute;
            document.getElementById('hour').value = task.hour;
            document.getElementById('day').value = task.dayOfMonth;
            document.getElementById('month').value = task.month;
            document.getElementById('dayOfWeek').value = task.dayOfWeek;
            
            editingIndex = index;
            submitBtn.textContent = 'Update Task';
            cancelBtn.style.display = 'inline-block';
        }
        
        // Delete task
        async function deleteTask(index) {
            if (confirm('Are you sure you want to delete this task?')) {
                tasks.splice(index, 1);
                try {
                    await saveTasks();
                    showStatus('Task deleted', true);
                } catch (error) {
                    showStatus('Error deleting task', false);
                }
                renderTasks();
            }
        }
        
        // Render tasks table
        function renderTasks() {
            tasksTable.innerHTML = '';
            
            tasks.forEach((task, index) => {
                const row = document.createElement('tr');
                
                row.innerHTML = `
                    <td>${task.name}</td>
                    <td>${task.minute}</td>
                    <td>${task.hour}</td>
                    <td>${task.dayOfMonth}</td>
                    <td>${task.month}</td>
                    <td>${task.dayOfWeek}</td>
                    <td class="task-actions">
                        <button class="edit-btn" onclick="editTask(${index})">Edit</button>
                        <button class="delete-btn" onclick="deleteTask(${index})">Delete</button>
                    </td>
                `;
                
                tasksTable.appendChild(row);
            });
        }
        
        // Save tasks to ESP32
        async function saveTasks() {
            try {
                const response = await fetch('/save-tasks', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(tasks)
                });
                
                if (!response.ok) {
                    throw new Error('Failed to save tasks');
                }
                
                return true;
            } catch (error) {
                console.error('Error saving tasks:', error);
                return false;
            }
        }
        
        // Show status message
        function showStatus(message, isSuccess) {
            statusDiv.textContent = message;
            statusDiv.className = isSuccess ? 'status success' : 'status error';
            statusDiv.style.display = 'block';
            
            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 3000);
        }
    </script>
</body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H