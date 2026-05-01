import net from 'node:net';
import { spawn } from 'node:child_process';
import process from 'node:process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const frontendDir = path.resolve(__dirname, '../frontend');

const PORT = 1745;

const npmExe = path.normalize(process.argv[2] || 'npm');

function isPortInUse(port) {
    return new Promise((resolve) => {
        const server = net.createServer();
        server.once('error', (err) => {
            if (err.code === 'EADDRINUSE') resolve(true);
            else resolve(false);
        });
        server.once('listening', () => {
            server.close();
            resolve(false);
        });
        server.listen(port, '127.0.0.1');
    });
}

async function main() {
    const inUse = await isPortInUse(PORT);

    if (inUse) {
        console.log(`[Shine] Vite server is already running on port ${PORT}. Skipping...`);
        process.exit(0);
    }

    console.log(`[Shine] Port ${PORT} is free. Starting Vite dev server...`);

    if (process.platform === 'win32') {
        const child = spawn('cmd.exe', ['/c', 'start', 'Shine Dev Server', npmExe, 'run', 'dev'], {
            cwd: frontendDir,
            detached: true,
            stdio: 'ignore'
        });

        child.unref();
    } else {
        const child = spawn('sh', ['-c', `"${npmExe}" run dev > /dev/null 2>&1 &`], {
            cwd: frontendDir,
            detached: true,
            stdio: 'ignore'
        });
        child.unref();
    }

    setTimeout(() => process.exit(0), 500);
}

main();