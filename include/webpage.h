#pragma once

const char* webpage = R"V0G0N(<!DOCTYPE html>
<html>
  <head>
    <title>Mini GPU Server</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css" />
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #f4f4f4;
        margin: 0;
        padding: 20px;
      }

      .container {
        max-width: 700px;
        margin: 0 auto;
        background-color: #fff;
        padding: 20px;
        border-radius: 4px;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
      }

      h1 {
        color: #333;
        text-align: center;
        margin-bottom: 20px;
      }

      h2 {
        color: #666;
        margin-bottom: 10px;
      }

      p {
        margin: 5px 0;
      }

      table {
        border-collapse: collapse;
        width: 100%;
        margin-bottom: 20px;
      }

      th,
      td {
        text-align: left;
        padding: 8px;
        border-bottom: 1px solid #ddd;
      }

      td:nth-child(2) {
        width: 70px; /* Adjust the width as needed */
        text-align: right;
      }

      th {
        background-color: #f2f2f2;
      }

      .button {
        display: inline-block;
        padding: 8px 16px;
        margin-right: 5px;
        font-size: 14px;
        border: none;
        border-radius: 4px;
        background-color: #4caf50;
        color: #fff;
        cursor: pointer;
      }

      .button:hover {
        background-color: #45a049;
      }

      .button-draw {
        background-color: #2196f3;
      }

      .button-draw:hover {
        background-color: #0b7dda;
      }

      .button-secondary {
        background-color: #f2f2f2;
        color: #333;
      }

      .button-secondary:hover {
        background-color: #ddd;
      }

      .button-wipe {
        background-color: #f44336;
      }

      .button-wipe:hover {
        background-color: #d32f2f;
      }

      input[type="file"] {
        display: none;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Mini GPU Server</h1>

      <h2>Monitor Details</h2>
      <p><strong>Resolution:</strong> <span id="resolution"></span></p>

      <h2>Storage Details</h2>
      <p><strong>Total space:</strong> <span id="totalSpace"></span></p>
      <p><strong>Used space:</strong> <span id="usedSpace"></span></p>

      <h2>File Structure</h2>
      <table id="fileStructure">
        <tr>
          <th>Name</th>
          <th>Size</th>
          <th>Action</th>
        </tr>
      </table>

      <h2>File Operations</h2>
      <input type="file" id="fileInput" style="display: none" />
      <button class="button" onclick="uploadFile()">Upload</button>
      <button class="button button-secondary" onclick="createDirectory()">Create Directory</button>
      <button class="button button-wipe" onclick="wipeSDCard()">Wipe SD</button>
    </div>

    <script>
      var currentDir = "/";

      // Function to update storage details
      function updateStorageDetails() {
        fetch("/storage")
          .then((response) => response.json())
          .then((data) => {
            document.getElementById("totalSpace").textContent = data.total + " KB";
            document.getElementById("usedSpace").textContent = data.used + " KB";
          })
          .catch((error) => console.log(error));
      }

      // Function to update file structure
      function updateFileStructure(dirName) {
        if (dirName === undefined) {
          dirName = currentDir;
        }

        // Fetch the file structure from the server
        let fileStructure = [];
        fetch("/listDir?dir=" + dirName)
          .then((response) => response.json())
          .then((data) => {
            fileStructure = data;

            var table = document.getElementById("fileStructure");
            while (table.rows.length > 1) {
              table.deleteRow(1);
            }

            // Add ".." directory to navigate to the parent directory
            if (dirName !== "/") {
              var row = table.insertRow();
              var nameCell = row.insertCell();
              var sizeCell = row.insertCell();
              var actionCell = row.insertCell();

              var icon = document.createElement("i");
              icon.style.marginRight = "10px";
              nameCell.style.padding = "20px";
              icon.className = "fas fa-folder";
              icon.style.color = "gray";
              nameCell.style.cursor = "pointer";
              nameCell.addEventListener("click", function () {
                navigateUp();
              });
              nameCell.appendChild(icon);

              var name = document.createElement("span");
              name.textContent = "..";
              nameCell.appendChild(name);

              sizeCell.textContent = "";

              actionCell.textContent = ""; // No action buttons for the parent directory
            }

            if (fileStructure.length > 0)
              fileStructure.forEach(function (file) {
                var row = table.insertRow();
                var nameCell = row.insertCell();
                var sizeCell = row.insertCell();
                var actionCell = row.insertCell();

                var icon = document.createElement("i");
                icon.style.marginRight = "10px";
                nameCell.style.padding = "20px";
                if (file.type === "dir") {
                  icon.className = "fas fa-folder";
                  nameCell.style.cursor = "pointer";
                  nameCell.addEventListener("click", function () {
                    navigate(file.name);
                  });
                } else {
                  icon.className = "fas fa-file";
                }
                nameCell.appendChild(icon);

                var name = document.createElement("span");
                name.textContent = file.name;
                nameCell.appendChild(name);

                sizeCell.textContent = file.size.toString().padStart(6, " ") + " KB";

                var filePath = currentDir + file.name;

                var drawButton = document.createElement("button");
                drawButton.textContent = "Draw";
                drawButton.className = "button button-draw";
                drawButton.addEventListener("click", function () {
                  handleDraw(filePath);
                });
                actionCell.appendChild(drawButton);

                var renameButton = document.createElement("button");
                renameButton.textContent = "Rename";
                renameButton.className = "button button-secondary";
                renameButton.addEventListener("click", function () {
                  handleRename(filePath);
                });
                actionCell.appendChild(renameButton);

                var deleteButton = document.createElement("button");
                deleteButton.textContent = "Delete";
                deleteButton.className = "button button-secondary";
                deleteButton.addEventListener("click", function () {
                  handleDelete(filePath);
                });
                actionCell.appendChild(deleteButton);

                var downloadButton = document.createElement("button");
                downloadButton.textContent = "Download";
                downloadButton.className = "button button-secondary";
                downloadButton.addEventListener("click", function () {
                  handleDownload(filePath);
                });
                actionCell.appendChild(downloadButton);
              });
          })
          .catch((error) => console.log(error));
      }

      // Function to navigate to the parent directory
      function navigateUp() {
        currentDir = currentDir.slice(0, -1);
        var lastSlashIndex = currentDir.lastIndexOf("/");
        if (lastSlashIndex !== -1) {
          currentDir = currentDir.slice(0, lastSlashIndex + 1);
          updateFileStructure(currentDir);
        }
      }

      // Function to handle navigation and update the file structure
      function navigate(folderName) {
        console.log("Navigating inside folder:", folderName);
        currentDir += folderName + "/";
        updateFileStructure(currentDir);
      }

      // Function to handle file upload
      function uploadFile() {
        var fileInput = document.getElementById("fileInput");
        fileInput.addEventListener("change", function () {
          var formData = new FormData();
          formData.append("file", fileInput.files[0]);

          fetch("/update?dir=" + currentDir, {
            method: "POST",
            body: formData,
          })
            .then((response) => response.json())
            .then((data) => {
              console.log("File uploaded successfully");
              updateFileStructure();
            })
            .catch((error) => console.log(error));
        });

        fileInput.click();
      }

      // Function to handle directory creation
      function createDirectory() {
        var dirName = prompt("Enter directory name");
        if (dirName !== null) {
          var dirPath = currentDir + dirName;
          console.log("Creating directory:", dirPath);
          fetch("/createDir?dir=" + dirPath)
            .then((response) => response.json())
            .then((data) => {
              console.log("Directory created successfully");
              updateFileStructure();
            })
            .catch((error) => console.log(error));
        }
      }

      // Function to wipe the SD card
      function wipeSDCard() {
        if (confirm("Are you sure you want to wipe the SD card?")) {
          fetch("/wipe")
            .then((response) => response.json())
            .then((data) => {
              console.log("SD card wiped successfully");
              updateFileStructure();
            })
            .catch((error) => console.log(error));
        }
      }

      // Function to handle image drawing
      function handleDraw(filename) {
        console.log(`Drawing image "${filename}"`);
        fetch("/draw?file=" + filename)
          .then((response) => response.json())
          .then((data) => {
            console.log("File sent for drawing");
            updateFileStructure();
          })
          .catch((error) => console.log(error));
      }

      // Function to handle file rename
      function handleRename(filename) {
        console.log(`Renaming file "${filename}"`);

        var newFilename = prompt("Enter new file name");
        if (newFilename === null) {
          return;
        }

        newFilename = currentDir + newFilename;

        fetch("/rename?old=" + filename + "&new=" + newFilename)
          .then((response) => response.json())
          .then((data) => {
            console.log("File renamed successfully");
            updateFileStructure();
          })
          .catch((error) => console.log(error));
      }

      // Function to handle file deletion
      function handleDelete(filename) {
        console.log(`Deleting file "${filename}"`);
        fetch("/delete?file=" + filename)
          .then((response) => response.json())
          .then((data) => {
            console.log("File deleted successfully");
            updateFileStructure();
          })
          .catch((error) => console.log(error));
      }

      // Function to handle file download
      function handleDownload(filename) {
        console.log(`Downloading file "${filename}"`);
        fetch("/download?file=" + filename)
          .then((response) => response.blob())
          .then((blob) => {
            var url = window.URL.createObjectURL(blob);
            var a = document.createElement("a");
            a.href = url;
            a.download = filename;
            a.click();
          })
          .catch((error) => console.log(error));
      }

      // Function to update monitor details
      function updateMonitorDetails() {
        fetch("/monitor")
          .then((response) => response.json())
          .then((data) => {
            document.getElementById("resolution").textContent = data.width + "x" + data.height;
          })
          .catch((error) => console.log(error));
      }

      // Update storage details and file structure when the page loads
      window.addEventListener("load", function () {
        updateStorageDetails();
        updateFileStructure("/");
        updateMonitorDetails();
      });
    </script>
  </body>
</html>
)V0G0N";
