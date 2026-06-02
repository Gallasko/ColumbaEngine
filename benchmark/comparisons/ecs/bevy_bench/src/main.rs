//! Standalone Bevy ECS bench — emits CSV in the shared format.
//!
//! Bevy ECS is archetype-based, like flecs. The crate `bevy_ecs` is usable
//! without the rest of Bevy, so this benchmark stays focused on the ECS itself
//! (no rendering, no scheduler, no app loop).
//!
//! Mirrors the 7 scenarios defined in `../scenarios.h`:
//!   - create_empty / create_bulk
//!   - create_1comp / create_2comp
//!   - iterate_read_1comp / iterate_write_1comp / iterate_apply_velocity
//!
//! Idiomatic APIs used:
//!   - world.spawn(())                            single empty entity
//!   - world.spawn_batch((0..n).map(|_| (...)))   bulk
//!   - world.query::<&T>().iter(&world)           read
//!   - world.query::<&mut T>().iter_mut(&mut w)   write

use bevy_ecs::prelude::*;
use std::fs::File;
use std::io::Write;
use std::time::Instant;

const FIXED_DT: f32 = 0.016;

#[derive(Component, Default, Clone, Copy)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Component, Default, Clone, Copy)]
struct Velocity {
    vx: f32,
    vy: f32,
}

// ---------- scenarios ----------

fn run_create_empty(n: usize) -> u64 {
    let mut world = World::new();
    let t0 = Instant::now();
    for _ in 0..n {
        world.spawn(());
    }
    t0.elapsed().as_nanos() as u64
}

fn run_create_bulk(n: usize) -> u64 {
    let mut world = World::new();
    let t0 = Instant::now();
    let _: Vec<_> = world.spawn_batch((0..n).map(|_| ())).collect();
    t0.elapsed().as_nanos() as u64
}

fn run_create_1comp(n: usize) -> u64 {
    let mut world = World::new();
    let t0 = Instant::now();
    for _ in 0..n {
        world.spawn(Position::default());
    }
    t0.elapsed().as_nanos() as u64
}

fn run_create_2comp(n: usize) -> u64 {
    let mut world = World::new();
    let t0 = Instant::now();
    for _ in 0..n {
        world.spawn((Position::default(), Velocity::default()));
    }
    t0.elapsed().as_nanos() as u64
}

fn run_iterate_read_1comp(n: usize) -> u64 {
    let mut world = World::new();
    for _ in 0..n {
        world.spawn(Position::default());
    }
    let mut q = world.query::<&Position>();

    let t0 = Instant::now();
    let mut sink: f32 = 0.0;
    for p in q.iter(&world) {
        sink += p.x;
    }
    let ns = t0.elapsed().as_nanos() as u64;
    // Prevent dead-code elimination of the sum.
    std::hint::black_box(sink);
    ns
}

fn run_iterate_write_1comp(n: usize) -> u64 {
    let mut world = World::new();
    for _ in 0..n {
        world.spawn(Position::default());
    }
    let mut q = world.query::<&mut Position>();

    let t0 = Instant::now();
    for mut p in q.iter_mut(&mut world) {
        p.x += 1.0;
        p.y += 1.0;
    }
    t0.elapsed().as_nanos() as u64
}

fn run_iterate_apply_velocity(n: usize) -> u64 {
    let mut world = World::new();
    for _ in 0..n {
        world.spawn((Position::default(), Velocity::default()));
    }
    let mut q = world.query::<(&mut Position, &Velocity)>();

    let t0 = Instant::now();
    for (mut p, v) in q.iter_mut(&mut world) {
        p.x += v.vx * FIXED_DT;
        p.y += v.vy * FIXED_DT;
    }
    t0.elapsed().as_nanos() as u64
}

// ---------- scenario registry ----------

#[derive(Copy, Clone)]
struct ScenarioDef {
    name: &'static str,
    run: fn(usize) -> u64,
}

const SCENARIOS: &[ScenarioDef] = &[
    ScenarioDef { name: "create_empty",            run: run_create_empty },
    ScenarioDef { name: "create_bulk",             run: run_create_bulk },
    ScenarioDef { name: "create_1comp",            run: run_create_1comp },
    ScenarioDef { name: "create_2comp",            run: run_create_2comp },
    ScenarioDef { name: "iterate_read_1comp",      run: run_iterate_read_1comp },
    ScenarioDef { name: "iterate_write_1comp",     run: run_iterate_write_1comp },
    ScenarioDef { name: "iterate_apply_velocity",  run: run_iterate_apply_velocity },
];

// ---------- peak RSS (Linux) ----------

#[cfg(target_os = "linux")]
fn peak_rss_kb() -> i64 {
    // /proc/self/status VmHWM is the high-water mark in KB.
    use std::io::Read;
    let mut s = String::new();
    if File::open("/proc/self/status")
        .and_then(|mut f| f.read_to_string(&mut s))
        .is_err()
    {
        return 0;
    }
    for line in s.lines() {
        if let Some(rest) = line.strip_prefix("VmHWM:") {
            return rest
                .trim()
                .split_whitespace()
                .next()
                .and_then(|n| n.parse::<i64>().ok())
                .unwrap_or(0);
        }
    }
    0
}

#[cfg(not(target_os = "linux"))]
fn peak_rss_kb() -> i64 {
    0
}

// ---------- CLI ----------

struct Options {
    scenarios: Vec<&'static str>,
    counts: Vec<usize>,
    runs: u32,
    warmup: u32,
    output: Option<String>,
    append: bool,
    engine: String,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            scenarios: SCENARIOS.iter().map(|s| s.name).collect(),
            counts: vec![1_000, 10_000, 100_000, 1_000_000],
            runs: 5,
            warmup: 1,
            output: None,
            append: false,
            engine: "bevy".to_string(),
        }
    }
}

fn print_help(argv0: &str) {
    eprintln!(
        "Usage: {} [options]
  --scenarios=all|<name,name,...>   default: all
  --counts=1000,10000,...           default: 1k,10k,100k,1M
  --runs=N                          measured runs (default 5)
  --warmup=N                        untimed warmups (default 1)
  --output=PATH                     CSV path (default: stdout)
  --append                          skip CSV header
  --engine=NAME                     override engine label (default: bevy)",
        argv0
    );
}

fn parse_args() -> Result<Options, String> {
    let mut opts = Options::default();
    let args: Vec<String> = std::env::args().collect();

    for a in args.iter().skip(1) {
        if let Some(v) = a.strip_prefix("--scenarios=") {
            if v == "all" {
                opts.scenarios = SCENARIOS.iter().map(|s| s.name).collect();
            } else {
                opts.scenarios.clear();
                for name in v.split(',') {
                    if let Some(s) = SCENARIOS.iter().find(|s| s.name == name) {
                        opts.scenarios.push(s.name);
                    } else {
                        return Err(format!("unknown scenario: {}", name));
                    }
                }
            }
        } else if let Some(v) = a.strip_prefix("--counts=") {
            opts.counts = v
                .split(',')
                .map(|s| s.parse::<usize>().map_err(|e| e.to_string()))
                .collect::<Result<_, _>>()?;
        } else if let Some(v) = a.strip_prefix("--runs=") {
            opts.runs = v.parse().map_err(|e: std::num::ParseIntError| e.to_string())?;
        } else if let Some(v) = a.strip_prefix("--warmup=") {
            opts.warmup = v.parse().map_err(|e: std::num::ParseIntError| e.to_string())?;
        } else if let Some(v) = a.strip_prefix("--output=") {
            opts.output = Some(v.to_string());
        } else if a == "--append" {
            opts.append = true;
        } else if let Some(v) = a.strip_prefix("--engine=") {
            opts.engine = v.to_string();
        } else if a == "--help" || a == "-h" {
            print_help(&args[0]);
            return Err(String::new());
        } else {
            return Err(format!("unknown arg: {}", a));
        }
    }

    Ok(opts)
}

// ---------- main ----------

fn run_scenario(name: &str, n: usize) -> u64 {
    SCENARIOS
        .iter()
        .find(|s| s.name == name)
        .map(|s| (s.run)(n))
        .unwrap_or(0)
}

fn main() {
    let opts = match parse_args() {
        Ok(o) => o,
        Err(msg) => {
            if !msg.is_empty() {
                eprintln!("{}", msg);
            }
            std::process::exit(1);
        }
    };

    let mut out: Box<dyn Write> = match opts.output.as_deref() {
        None => Box::new(std::io::stdout()),
        Some(path) => {
            let f = std::fs::OpenOptions::new()
                .write(true)
                .create(true)
                .truncate(!opts.append)
                .append(opts.append)
                .open(path)
                .unwrap_or_else(|e| {
                    eprintln!("cannot open {}: {}", path, e);
                    std::process::exit(1);
                });
            Box::new(f)
        }
    };

    if !opts.append {
        writeln!(
            out,
            "engine,scenario,entity_count,run_id,total_ns,per_entity_ns,peak_rss_kb"
        )
        .unwrap();
    }

    eprintln!(
        "bevy ecs bench — runs={} warmup={} scenarios={} counts={}",
        opts.runs,
        opts.warmup,
        opts.scenarios.len(),
        opts.counts.len()
    );

    for scen in &opts.scenarios {
        for &n in &opts.counts {
            for _ in 0..opts.warmup {
                let _ = run_scenario(scen, n);
            }
            for r in 0..opts.runs {
                let ns = run_scenario(scen, n);
                let per_entity = if n > 0 { ns as f64 / n as f64 } else { 0.0 };
                writeln!(
                    out,
                    "{},{},{},{},{},{:.4},{}",
                    opts.engine,
                    scen,
                    n,
                    r,
                    ns,
                    per_entity,
                    peak_rss_kb()
                )
                .unwrap();
            }
            eprintln!("  {:<24} n={:<8} done", scen, n);
        }
    }
}
