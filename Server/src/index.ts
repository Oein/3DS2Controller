// tcp client to port 8050

import net from "net";
import path from "path";
import { existsSync, readFileSync, writeFileSync } from "fs";
import { ChildProcess, execFile } from "child_process";

const pkg = (process as any).pkg;
const basePath = pkg ? path.dirname(process.execPath) : __dirname;
const configPath = path.join(basePath, "config.json");
const vigemPath = path.join(basePath, "vigem.exe");

let client: net.Socket;
let child: ChildProcess;

const anyKey2Exit = () => {
  console.log("Press any key to exit");
  process.stdin.setRawMode(true);
  process.stdin.resume();
  process.stdin.on("data", () => {
    if (client && !client.destroyed) client.destroy();
    if (child && !child.killed) child.kill();
    process.exit();
  });
};

const main = () => {
  client = new net.Socket();

  const config = JSON.parse(readFileSync(configPath).toString());

  const parseData = (data: Buffer) => {
    // console.log(data);
    if (data.length != 3) return;

    const b = (index: number, bitIndex: number) => {
      return (data[index] & (1 << bitIndex)) != 0;
    };

    const bx = (index: number, bitIndex: number) => {
      return b(index, bitIndex) ? 1 : 0;
    };

    const Parsed = {
      A: b(0, 0),
      B: b(0, 1),
      X: b(0, 2),
      Y: b(0, 3),
      L: b(0, 4),
      R: b(0, 5),
      ZL: b(0, 6),
      ZR: b(0, 7),
      SELECT: b(1, 0),
      START: b(1, 1),
      DUP: b(1, 2),
      DDOWN: b(1, 3),
      DLEFT: b(1, 4),
      DRIGHT: b(1, 5),
      CUP: b(1, 6),
      CDOWN: b(1, 7),
      CLEFT: b(2, 0),
      CRIGHT: b(2, 1),
      LRIGHT: b(2, 2),
      LLEFT: b(2, 3),
      LUP: b(2, 4),
      LDOWN: b(2, 5),
    };

    const maxv = 1;
    const minv = -1;

    const dt = (v: boolean, data: any) => data + (v ? "+" : "-");

    const getNotiation = (
      up: boolean,
      down: boolean,
      left: boolean,
      right: boolean
    ) => {
      if (up && !down && !left && !right) return "8";
      if (up && !down && !left && right) return "9";
      if (up && !down && left && !right) return "7";
      if (!up && !down && left && !right) return "4";
      if (!up && !down && !left && right) return "6";
      if (!up && down && !left && !right) return "2";
      if (!up && down && !left && right) return "3";
      if (!up && down && left && !right) return "1";
      return "5";
    };

    const mf = (dt: string, pre: string) => pre + dt;

    const getDPad = () =>
      mf(
        getNotiation(Parsed.DUP, Parsed.DDOWN, Parsed.DLEFT, Parsed.DRIGHT),
        "D"
      );

    // console.log("AN", Parsed.ANALOGLSTICK);
    const getLStick = () =>
      mf(
        getNotiation(Parsed.LUP, Parsed.LDOWN, Parsed.LLEFT, Parsed.LRIGHT),
        "L"
      );

    const getRStick = () =>
      mf(
        getNotiation(Parsed.CUP, Parsed.CDOWN, Parsed.CLEFT, Parsed.CRIGHT),
        "R"
      );

    const command = `${[
      dt(Parsed.A, "A"),
      dt(Parsed.B, "B"),
      dt(Parsed.X, "X"),
      dt(Parsed.Y, "Y"),
      dt(Parsed.L, "LB"),
      dt(Parsed.R, "RB"),
      dt(Parsed.ZL, "L"),
      dt(Parsed.ZR, "R"),
      dt(Parsed.SELECT, "BACK"),
      dt(Parsed.START, "START"),
      getDPad(),
      getLStick(),
      getRStick(),
    ]
      .filter((x) => x != "")
      .join(" ")}`;

    console.log("> " + command);
    child.stdin?.write(command + "\n");
  };

  client.on("data", (data) => {
    // console.log("DATA", data);
    parseData(data);
  });

  // on disconnect
  client.on("close", () => {
    console.log("CONNECTION CLOSED");
    child.stdin?.write("!\n");

    // wait key press to exit
    anyKey2Exit();
  });

  client.on("error", (err) => {
    console.log("ERROR", err);
  });

  console.log("CONNECTING TO 3DS");
  client.connect(config["3DS_PORT"], config["3DS_IP"], () => {
    console.log("CONNECTED TO 3DS");
    child = execFile(vigemPath);
    child.on("exit", (code, signal) => {
      console.log("Vigem exited with code", code, signal);
      anyKey2Exit();
    });

    // anyKey2Exit();
  });
};

import { createWriteStream } from "fs";
import axios from "axios";

async function downloadFile(fileUrl: string, outputLocationPath: string) {
  const writer = createWriteStream(outputLocationPath);

  return axios({
    method: "get",
    url: fileUrl,
    responseType: "stream",
  }).then((response) => {
    //ensure that the user can call `then()` only when the file has
    //been downloaded entirely.

    return new Promise((resolve, reject) => {
      response.data.pipe(writer);
      let error: any = null;
      writer.on("error", (err) => {
        error = err;
        writer.close();
        reject(err);
      });
      writer.on("close", () => {
        writer.close();
        writer.end();
        if (!error) {
          setTimeout(() => {
            resolve(true);
          }, 100);
        }
        //no need to call the reject here, as it will have been called in the
        //'error' stream;
      });
    });
  });
}
const ensureVigem = async () => {
  if (existsSync(vigemPath)) return;
  console.log("Downloading vigem.exe");
  const file = createWriteStream(vigemPath);
  const URL =
    "https://github.com/henrikvik/vigem-interface/releases/download/1.0/vigem-interface.exe";
  return new Promise<void>((resolve, reject) => {
    downloadFile(URL, vigemPath).then(() => {
      console.log("Downloaded vigem.exe");
      anyKey2Exit();
    });
  });
};

if (!existsSync(configPath)) {
  writeFileSync(
    configPath,
    JSON.stringify(
      {
        "3DS_IP": "127.0.0.1",
        "3DS_PORT": 80,
      },
      null,
      2
    )
  );
  console.error("Please configure the config.json file");
  anyKey2Exit();
} else ensureVigem().then(main).catch(console.error);
