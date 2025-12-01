//set up server
import express from "express";
import { createServer } from "http";
import { WebSocketServer } from "ws";
import path from "path";
import { fileURLToPath } from "url";

//additional setup
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
app.use(express.json());
app.use(express.static("public"));
const server = createServer(app);
const wss = new WebSocketServer({ server });

app.use("/", express.static("public"));

const clients = new Set(); //a js storage object, similiar to array, but will prevent duplicate data

//determine what needs to be kept here - initial values for jellyfish motor off/on, distance for the eel and the angler
const serverState = {
  //is jellyfish on?
  jellyOn: false,

  // Example 2 state - REMOVE if we don't need
  //   brightness: 128,
  //   pulseRate: 50,
  //   servoAngle: 90,

  //angler distance
  anglerDistance: 122,
};

//helpter function for brodcasting data to clients
function broadcast(data) {
  const message = JSON.stringify(data);
  clients.forEach((client) => {
    if (client.readyState === 1) {
      client.send(message);
    }
  });
}

//set up web socket !

wss.on("connection", (ws, req) => {
  //ws is the connected client
  console.log("New client connected");
  clients.add(ws); //add the connected client to the clients set

  // Send current state to newly connected client
  ws.send(
    JSON.stringify({
      type: "initialState",
      state: serverState,
    })
  );

  //add these event listeners to the client
  ws.on("message", (incomingData) => {
    try {
      const data = JSON.parse(incomingData); //incomingData string as json
      console.log("Received:", data); //peek at the incoming data

      // Example 1: Button toggle - MODIFY THIS FOR JELLY CODE
      if (data.type === "jellyPress") {
        serverState.jellyOn = !serverState.jellyOn; //toggle the led state
        console.log("Jelly toggled to:", serverState.jellyOn);
        broadcast({ type: "jellyState", value: serverState.jellyOn });
      }

      //handle angler data - THIS IS NOT WORKING, NEED TO TROUBLESHOOT - SB 11.30
      if (data.type === "brightness_angler") {
        serverState.anglerDistance = data.value;
        broadcast({ type: "angler", value: data.value });
      }
    } catch (error) {
      console.error("Error parsing message:", error);
    }
  });

  ws.on("close", () => {
    console.log("Client disconnected");
    clients.delete(ws);
  });

  ws.on("error", (error) => {
    console.error("WebSocket error:", error);
  });
});

//'port' variable allows for deployment
const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
