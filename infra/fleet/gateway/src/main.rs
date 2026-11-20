//! `neurone-fleet-gateway`: see the library. Configuration is the environment, and the key is read from a file so it can be a secret.
//!
//! * `GATEWAY_KEY_FILE`: two lines, the key configuration (hex) then the private key (hex); `--generate <file>` writes one.
//! * `GATEWAY_LISTEN`: default `0.0.0.0:8080`. `INGEST_ADDR`: `host:port` of the ingest service.

use neurone_fleet_gateway::{serve, Config};
use std::io::Read;

fn die(msg: &str) -> ! {
    eprintln!("{msg}");
    std::process::exit(2)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(String::as_str) == Some("--generate") {
        let path = args.get(2).unwrap_or_else(|| die("--generate needs a file name"));
        let mut seed = [0u8; 32];
        std::fs::File::open("/dev/urandom").and_then(|mut f| f.read_exact(&mut seed)).unwrap_or_else(|_| die("no random source"));
        // The seed goes to the core as bytes and the private key comes back as bytes, never as text (`SWR-MG-32`); only the
        // file, which is the configuration, holds it as hex.
        let (v, beside) = neurone_fleet_gateway::ask_with_secrets(serde_json::json!({ "op": "ohttpGenerateKey", "seed": { "$secret": 0 }, "keyId": 1 }), &[&seed])
            .unwrap_or_else(|e| die(&e));
        seed.fill(0);
        let private_key: String = beside.first().unwrap_or_else(|| die("the core gave no private key")).iter().map(|b| format!("{b:02x}")).collect();
        use std::os::unix::fs::OpenOptionsExt;
        let mut f = std::fs::OpenOptions::new().write(true).create_new(true).mode(0o600).open(path).unwrap_or_else(|e| die(&format!("{path}: {e}")));
        use std::io::Write;
        writeln!(f, "{}\n{}", v["keyConfig"].as_str().unwrap(), private_key).unwrap();
        println!("key configuration (publish this, and set it in the apps): {}", v["keyConfig"].as_str().unwrap());
        return;
    }
    let key_file = std::env::var("GATEWAY_KEY_FILE").unwrap_or_else(|_| die("GATEWAY_KEY_FILE is not set"));
    let text = std::fs::read_to_string(&key_file).unwrap_or_else(|e| die(&format!("{key_file}: {e}")));
    let mut lines = text.lines();
    let (Some(key_config_hex), Some(private_key_hex)) = (lines.next(), lines.next()) else { die("the key file needs two lines") };
    let ingest = std::env::var("INGEST_ADDR").unwrap_or_else(|_| die("INGEST_ADDR is not set"));
    let listen = std::env::var("GATEWAY_LISTEN").unwrap_or_else(|_| "0.0.0.0:8080".into());
    let cfg = Config { key_config_hex: key_config_hex.trim().into(), private_key_hex: private_key_hex.trim().into(), ingest };
    let (addr, counters) = serve(&listen, cfg).unwrap_or_else(|e| die(&format!("{listen}: {e}")));
    eprintln!("gateway listening on {addr}");
    loop {
        std::thread::sleep(std::time::Duration::from_secs(3600));
        let c = &counters;
        eprintln!(
            "forwarded={} refused={} ingest_failed={}",
            c.forwarded.load(std::sync::atomic::Ordering::Relaxed),
            c.refused.load(std::sync::atomic::Ordering::Relaxed),
            c.ingest_failed.load(std::sync::atomic::Ordering::Relaxed)
        );
    }
}
