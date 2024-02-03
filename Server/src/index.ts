// tcp client to port 8050

import net from "net";
import path from "path";
import { existsSync, readFileSync, writeFileSync } from "fs";
import { execFile } from "child_process";

const pkg = (process as any).pkg;
const configPath = pkg
  ? path.join(path.dirname(process.execPath), "config.json")
  : path.join(__dirname, "config.json");

const main = () => {
  const client = new net.Socket();
  var child = execFile(
    path.join(__dirname, "..", "assets", "vigem-interface.exe")
  );

  const config = JSON.parse(readFileSync(configPath).toString());

  let STICK_THRESHOLD = config.STICK_THRESHOLD;

  const parseData = (data: Buffer) => {
    if (data.length != 7) return;

    const b = (index: number, bitIndex: number) => {
      return (data[index] & (1 << bitIndex)) != 0;
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

      ANALOG: {
        X: Math.min(Math.max(data.readInt16LE(3), -150), 150),
        Y: Math.min(Math.max(data.readInt16LE(5), -150), 150),
      },
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

    const getLStick = () =>
      mf(
        getNotiation(
          Parsed.ANALOG.Y > STICK_THRESHOLD,
          Parsed.ANALOG.Y < -STICK_THRESHOLD,
          Parsed.ANALOG.X < -STICK_THRESHOLD,
          Parsed.ANALOG.X > STICK_THRESHOLD
        ),
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
      dt(Parsed.ZL, "LT"),
      dt(Parsed.ZR, "RT"),
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
    parseData(data);
  });

  // on disconnect
  client.on("close", () => {
    console.log("CONNECTION CLOSED");
    child.stdin?.write("!\n");

    // wait key press to exit
    process.stdin.setRawMode(true);
    process.stdin.resume();
    process.stdin.on("data", process.exit.bind(process, 0));
    console.log("Press any key to exit");
  });

  client.on("error", (err) => {
    console.log("ERROR", err);
  });

  client.connect(config["3DS_PORT"], config["3DS_IP"], () => {
    console.log("CONNECTED TO 3DS");
  });
};

if (!existsSync(configPath)) {
  writeFileSync(
    configPath,
    JSON.stringify(
      {
        STICK_THRESHOLD: 60,
        "3DS_IP": "127.0.0.1",
        "3DS_PORT": 80,
      },
      null,
      2
    )
  );
  console.error("Please configure the config.json file");
  console.log("Press any key to exit");

  process.stdin.setRawMode(true);
  process.stdin.resume();
  process.stdin.on("data", process.exit.bind(process, 0));
} else main();
