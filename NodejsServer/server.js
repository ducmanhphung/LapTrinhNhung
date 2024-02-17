const path = require('path');
const express = require('express');
const WebSocket = require('ws');

const app = express();

app.use('/static', express.static(path.join(__dirname, 'public')));

let clients = [];

const HTTP_PORT = 8000;
let devices = {
	relay_module1: { port: 8888 },
};

process.on('uncaughtException', (error, origin) => {
	console.log('----- Uncaught exception -----');
	console.log(error);
	console.log('----- Exception origin -----');
	console.log(origin);
	console.log('----- Status -----');
});

// Clients in browser
// Khởi tạo websocket server cho ứng dụng web để theo dõi hình ảnh từ camera trên cổng 8999
const wss = new WebSocket.Server({port: '8999'}, () => console.log(`WS Server is listening at 8999`));

wss.on('connection', ws => {
	ws.on('message', data => {
		if (ws.readyState !== ws.OPEN) return;
		// Thêm websocket này vào danh sách các clients
		clients.push(ws);
		try {
			// Nhận và xử lý dữ liệu được truyền từ web browser để điều khiển esp32camera (device)
			data = JSON.parse(data);
			if(data.operation === 'command') {
				if(devices[data.command.recipient]) {
					// Thêm command cho thiết bị, command sẽ được gửi đến thiết bị và xóa sau đó
					devices[data.command.recipient].command = data.command.message.key + '=' + data.command.message.value;
					console.log(devices[data.command.recipient].command);
				}
			}
		} catch (error) {}
	});
});


// Devices (esp32cam)
Object.entries(devices).forEach(([key]) => {
	//lặp lấy tất cả các key trong đối tượng devices

	// lấy thông tin device
	const device = devices[key];

	// tạo websocket server listen device
	const wssDevice = new WebSocket.Server({port: device.port}, () => console.log(`WS Server is listening at ${device.port}`));

	wssDevice.on('connection',(ws) => {

		// handle khi device (esp32cam) gửi dữ liệu đến server
		ws.on('message', data => {
			if (ws.readyState !== ws.OPEN) return;

			// device.command được thiết lập khi client (webbrowser) gửi về server
			if (device.command) {
				// gửi command đến device (esp32cam)
				ws.send(device.command);
				// comsume command
				device.command = null; // Consume
			}

			if (typeof data === 'object') {
				// lấy hình ảnh được gửi đến từ device(esp32cam)
				device.image = Buffer.from(Uint8Array.from(data)).toString('base64');
			} else {
				// lấy các thông tin gửi đến không phải là hình ảnh
				// chuyển dang dạng đối tượng
				// gán đối tượng cho device.peripherals
				device.peripherals = data.split(",").reduce((acc, item) => {
					const key = item.split("=")[0];
					const value = item.split("=")[1];
					acc[key] = value;
					return acc;
				}, {});
			}

			// gửi các thông các danh sách devices đến client
			clients.forEach(client => {
				client.send(JSON.stringify({ devices: devices }));
			});
		});
	});
});

// paths
app.get('/client',(_req,res)=>{ res.sendFile(path.resolve(__dirname,'./public/client.html')); });
// start server
app.listen(HTTP_PORT,()=>{ console.log(`HTTP server starting on ${HTTP_PORT}`); });