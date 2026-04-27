import { spawn } from "node:child_process";
import path from "node:path";
import url from "node:url";
import fs from "node:fs";
import process from "node:process";

const __dirname = path.dirname(url.fileURLToPath(import.meta.url));
const projectRoot = path.resolve(__dirname, "..");
const buildDir = path.join(projectRoot, "build");

function run(cmd, args, opts = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(cmd, args, { stdio: "inherit", shell: true, ...opts });
    child.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`${cmd} ${args.join(" ")} exited with code ${code}`));
    });
  });
}

async function capture(cmd, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"], shell: true });
    let out = "";
    let err = "";
    child.stdout.on("data", (d) => (out += String(d)));
    child.stderr.on("data", (d) => (err += String(d)));
    child.on("close", (code) => {
      if (code === 0) resolve({ out, err });
      else reject(new Error(`${cmd} ${args.join(" ")} exited with code ${code}\n${err}`));
    });
  });
}

function firstExisting(paths) {
  for (const p of paths) {
    try {
      if (p && fs.existsSync(p)) return p;
    } catch {
      // ignore
    }
  }
  return null;
}

async function resolveCMakeExe() {
  if (process.env.CMAKE && fs.existsSync(process.env.CMAKE)) return process.env.CMAKE;
  if (process.env.CMAKE_EXE && fs.existsSync(process.env.CMAKE_EXE)) return process.env.CMAKE_EXE;

  if (process.platform === "win32") {
    try {
      const { out } = await capture("where", ["cmake"]);
      const candidate = out
        .split(/\r?\n/g)
        .map((s) => s.trim())
        .filter(Boolean)[0];
      if (candidate && fs.existsSync(candidate)) return candidate;
    } catch {
      // ignore
    }

    const vsCandidates = [
      "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
      "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
      "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
      "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
      "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
      "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    ];
    const existing = firstExisting(vsCandidates);
    if (existing) return existing;
  }

  return "cmake";
}

fs.mkdirSync(buildDir, { recursive: true });

const cmake = await resolveCMakeExe();

if (process.platform === "win32" && cmake === "cmake") {
  try {
    await capture("cmake", ["--version"]);
  } catch {
    console.error(
      [
        "CMake was not found.",
        "",
        "Install CMake and make sure it's in PATH, or point this script to it:",
        "  setx CMAKE_EXE \"C:\\\\path\\\\to\\\\cmake.exe\"",
      ].join("\n")
    );
    process.exit(1);
  }
}

await run(cmake, ["-S", projectRoot, "-B", buildDir, "-DCMAKE_BUILD_TYPE=Release"]);
await run(cmake, ["--build", buildDir, "--config", "Release"]);

