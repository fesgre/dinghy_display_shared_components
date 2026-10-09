
# Dinghy IoT – System Architecture

## 1. Project Objective

Build a modular, distributed sailing instrumentation and boat monitoring
system using ESP32-C6 controllers, ESP-IDF, nanopb, Zenoh-Pico and NMEA 2000.

The system must operate autonomously without internet connectivity.
A smartphone provides either a local dashboard connection or an internet
hotspot during the initial development phase. A dedicated mobile router
and cloud connectivity may be added later.

The system is designed for a small sailing dinghy and must prioritize:
- Reliability and predictable failure behavior
- Low power consumption
- Loose coupling between subsystems
- Local-first operation
- Observability and diagnostics
- Expandability without redesigning existing zone controllers

## 2. Hardware and Software Platform

### Hardware
- ESP32-C6-DevKit-N8 for initial development
- Additional ESP32-based zonal controllers
- GPS/GNSS receiver
- Masthead wind sensor
- Optional IMU and heel/trim sensors
- Battery and electric motor monitoring
- Separate ESP32-based e-paper display controller
- Raspberry Pi may be added later for OpenCPN and higher-level processing
- NMEA 2000 physical interface connected to the boat's CAN bus

### Firmware
- ESP-IDF (not Arduino)
- C
- FreeRTOS tasks and queues
- nanopb for Protocol Buffers serialization
- Zenoh-Pico for distributed publish/subscribe messaging
- HTTP server for the local dashboard and health diagnostics
- NMEA 2000 protocol implementation using appropriate PGNs and CAN transport

Use ESP-IDF-native drivers and components wherever practical.
Keep hardware access behind clearly defined interfaces.

## 3. System Topology

                         Smartphone
                     /               \
              Dashboard           Wi-Fi hotspot
                  |                    |
                  |                    | Internet
                  |                    |
                  +------ Gateway -----+
                            |
                 ESP32-C6 Gateway
                 /       |         \
          Zenoh-Pico   HTTP       NMEA 2000
              |       Dashboard    CAN bus
         +----+----+                |
         |         |          Marine devices
      Mast ECU   Other zones
         |
     Wind sensors

The zonal controllers and gateway communicate over the local Wi-Fi network.
The gateway is the integration point for external interfaces, diagnostics
and optional internet services.

The e-paper controller subscribes to relevant local data and renders
navigation, wind, weather and system status.

The gateway must not require a Raspberry Pi, a mobile router or a cloud
service for core functionality.

## 4. Zonal Controller Architecture

Each zonal controller owns its local sensors and actuators.

Responsibilities:
- Initialize and supervise local hardware
- Acquire and validate sensor measurements
- Apply calibration and local filtering
- Timestamp measurements
- Track data quality and sensor health
- Serialize messages using nanopb
- Publish data through Zenoh-Pico
- Subscribe to relevant commands when required
- Implement local safe behavior if communication is lost

A zone controller must not depend on:
- The dashboard being available
- Internet access
- The gateway's HTTP server
- NMEA 2000 being operational

Avoid cross-zone hardware dependencies.

Suggested zones:
- Mast zone: apparent wind, wind direction, masthead sensors
- Navigation zone: GNSS, heading, speed over ground
- Boat-motion zone: IMU, heel and trim
- Energy zone: battery voltage/current, state of charge
- Motor zone: motor command, measured power and operating state

These zones are logical responsibilities, not a requirement for one MCU
per sensor. Consolidate hardware where appropriate.

## 5. Data Serialization: nanopb

Define all inter-controller application messages using .proto files.

Generate nanopb C structures and encode/decode functions from the schemas.
Do not hand-maintain generated code.

Each telemetry message should provide, where applicable:
- Schema/version identification
- Source controller ID
- Measurement timestamp
- Sequence number
- Measurement value and unit
- Validity and quality flags
- Sensor or controller diagnostic status

Use appropriate fixed-width types and bounded arrays.
Avoid unbounded allocations and unnecessary heap usage.

Keep units explicit and consistent. Prefer SI units internally.

Example message families:
- NavigationData
- WindData
- BoatMotionData
- BatteryData
- MotorData
- WeatherObservation
- WeatherForecast
- ControllerHealth
- SystemHealth
- DeviceCommand

Separate telemetry, configuration, commands and diagnostics.

nanopb is the application serialization format. Zenoh transports the
serialized payloads and provides topic-based distribution.

## 6. Zenoh-Pico Messaging

Use Zenoh-Pico for local publish/subscribe communication between
controllers, gateway and display.

Example key expressions:
- boat/navigation/position
- boat/navigation/sog
- boat/navigation/cog
- boat/navigation/heading
- boat/wind/apparent
- boat/wind/true
- boat/motion/heel
- boat/motion/trim
- boat/energy/battery
- boat/motor/status
- boat/weather/observation
- boat/weather/forecast
- boat/health/controller/{controller_id}
- boat/health/system
- boat/command/{controller_id}/#

Define topic naming, payload schemas, units, timestamps, update rates
and ownership centrally in project documentation.

Use bounded queues and explicit policies for overload:
- Latest-value telemetry may replace stale queued values.
- Events and commands must not be silently treated as ordinary telemetry.
- Commands require validation and appropriate acknowledgement.
- Sensor failures must be distinguishable from missing communication.

Do not implement a custom messaging protocol on top of Zenoh unless
there is a documented requirement.

Important: Zenoh-Pico is an embedded Zenoh implementation, not the
full zenohd router. Design the local topology using supported
Zenoh-Pico peer/session capabilities and verify interoperability
against the chosen Zenoh version.

## 7. Gateway Responsibilities

The ESP32-C6 gateway is the local integration controller.

It must:
1. Receive and publish relevant data through Zenoh-Pico.
2. Decode and validate nanopb payloads.
3. Maintain a current-value cache with timestamps and validity.
4. Translate internal navigation and instrument data to NMEA 2000 PGNs.
5. Receive supported NMEA 2000 PGNs and convert them to internal messages
   when bidirectional integration is required.
6. Host a local HTTP dashboard.
7. Expose system health and diagnostic information.
8. Acquire external weather data when internet is available.
9. Normalize external data into internal nanopb message types.
10. Publish weather and other external data through Zenoh-Pico.
11. Continue local operation when internet connectivity is lost.

Keep protocol translation separate from sensor acquisition and the
dashboard implementation.

Suggested software modules:
- gateway_main
- wifi_manager
- zenoh_manager
- telemetry_registry
- nmea2000_service
- weather_service
- http_dashboard
- health_monitor
- configuration_manager
- time_service

Use FreeRTOS tasks or event-driven components with clear ownership.
Avoid blocking network operations in time-critical CAN or sensor tasks.

## 8. NMEA 2000 Interface

NMEA 2000 is the marine instrument interface. Do not substitute NMEA 0183.

Implement the required NMEA 2000 functionality using a compatible
implementation and the appropriate Parameter Group Numbers (PGNs).

Requirements:
- CAN 2.0 extended 29-bit identifiers at 250 kbit/s
- NMEA 2000 address claiming and device identity
- Product information and supported-PGN handling as required
- Correct PGN definitions, field scaling, units and validity encoding
- Appropriate update intervals and timeout behavior
- Receive filtering and input validation
- Respect bus load and avoid unnecessary repeated transmissions

Select only PGNs justified by available data and the intended instruments.
Potential categories include GNSS position, COG/SOG, heading, apparent
wind, environmental data and battery status.

Do not invent PGN numbers or payload layouts. Verify them against the
applicable NMEA 2000 specifications and the chosen implementation.

Hardware requirements:
- A suitable CAN transceiver and appropriate bus protection
- Correct CAN wiring, backbone, drop cables and termination
- TWAI/CAN driver support verified for the target ESP32-C6 and ESP-IDF
- External controller hardware if the required peripheral functionality
  is not available

Never connect the MCU GPIO directly to the NMEA 2000 CAN bus.

Treat NMEA 2000 physical-layer requirements and device conformance as
separate from application-level software correctness.

## 9. Local Dashboard and Health Check

Host a small, responsive website directly on the gateway.

The dashboard must work when no internet connection is available.

Provide:
- Live navigation and wind data
- Battery and motor information
- Controller connectivity and last-seen timestamps
- Sensor validity and stale-data indicators
- Wi-Fi AP/STA state and internet availability
- Zenoh session and messaging status
- NMEA 2000/CAN status and transmit/receive counters
- External API status and age of weather data
- Uptime, reset reason and relevant error counters
- Firmware and schema versions

Use lightweight HTTP and a suitable live-update mechanism such as
Server-Sent Events or WebSockets if required.

Do not use cloud-hosted JavaScript, CSS or fonts as dashboard dependencies.

Provide a machine-readable health endpoint, for example /api/health,
and a human-readable status page.

A reachable HTTP server does not imply that sensors or the CAN bus
are healthy. Report component health separately.

## 10. Wi-Fi and Connectivity

The gateway supports:
- SoftAP for direct local access
- Station mode to connect to a smartphone hotspot
- AP+STA operation where supported by the ESP32-C6 and ESP-IDF version

Initial development uses the Android Moto G8 as the hotspot.

The phone can operate in one of two modes:
A. Connect to the gateway AP and display the dashboard.
B. Provide a hotspot for gateway internet access.

Do not assume the phone can provide its hotspot and simultaneously
remain connected as a Wi-Fi client to the gateway.

The gateway dashboard must remain locally available independently of
internet connectivity.

Define predictable behavior for:
- Hotspot unavailable at startup
- Hotspot lost during operation
- Wi-Fi reconnect attempts
- AP+STA channel constraints
- DNS and local dashboard addressing
- Internet access unavailable despite Wi-Fi association

Avoid unnecessary Wi-Fi reconnect loops and excessive power consumption.

## 11. External Web APIs and Weather

The gateway fetches external API data only when internet connectivity
is available.

Responsibilities:
- Use configurable API endpoints and credentials where required.
- Apply connection and request timeouts.
- Validate responses and handle malformed data.
- Convert responses into normalized internal data models.
- Publish normalized data as nanopb messages over Zenoh-Pico.
- Include source, observation/forecast time and retrieval time.
- Mark expired or unavailable data as stale or invalid.
- Use bounded retry and backoff policies.

Do not block local telemetry or NMEA 2000 operation while waiting for
an external API.

No API must be necessary for core navigation functionality.

## 12. Time and Data Quality

Distinguish:
- Measurement time
- Gateway reception time
- External observation time
- External data retrieval time

GNSS may provide the primary UTC time reference when available.
Define behavior before GNSS time synchronization is established.

Track message age, sequence gaps and invalid measurements.
Never silently substitute zero for unavailable sensor values.

Define stale-data thresholds per data type rather than using a single
global timeout.

## 13. Security and Fault Handling

Treat Wi-Fi and external API input as untrusted.

Requirements:
- Validate message lengths and decoded values.
- Bound queues, buffers and retry counts.
- Protect configuration and command endpoints appropriately.
- Do not expose secrets in logs or source control.
- Do not allow internet loss to trigger uncontrolled resets.
- Prevent stale telemetry from being presented as current.
- Use a watchdog strategy appropriate to each task.
- Define safe defaults for lost actuator commands.

The local network is not assumed to be secure merely because it is
hosted by the boat.

## 14. Future Cloud Connectivity

Cloud support is a future extension, not a prerequisite for the first
prototype.

Future topology:
- A mobile LTE/5G router provides the boat's shared WLAN and internet.
- The gateway joins the router in station mode.
- Local sensors and displays continue using the boat's local network.
- A remote Zenoh router (zenohd) can provide cloud connectivity.
- The gateway or an appropriate bridge forwards selected data.
- Cloud storage and analysis subscribe only to the required topics.

Do not assume Zenoh-Pico on the ESP32-C6 can replace zenohd.

Design an explicit cloud bridge or router connection compatible with
the selected Zenoh versions and supported transports.

Filter and aggregate high-rate data before sending it over a metered link.
Cloud unavailability must not interrupt local operations.

## 15. Transition to a Monorepo

The current firmware projects are separate ESP-IDF projects. The target
repository should bring the system firmware and shared application components
together while keeping every firmware application independently configurable
and buildable. This enables coordinated schema and component changes without
making the gateway and participant firmware a single ESP-IDF build target.

Suggested repository layout:

```text
dinghy-firmware/
  apps/
    gateway/
    participants/
      <zone>/
  components/
    boat_protocol/
    wifi_connect/
    telemetry/
    ...
  third_party/
    nanopb/                 # one pinned source, or use ESP-IDF component manager
  experiments/
    DinghyDisplay/
    hello_wifi/
    hello_wifi_participant/
    hello_world/
  docs/
    ARCHITECTURE.md
    PROTOCOL.md
```

Each directory under `apps/` remains a standalone ESP-IDF project with its own
`CMakeLists.txt`, `sdkconfig.defaults`, and project-specific configuration.
Each app uses the shared `components/` directory through ESP-IDF's component
search path. Build output stays local to each project and is not shared between
apps. Keep only one nanopb source/version in the repository; do not retain both
a top-level nanopb checkout and another vendored copy.

Move the current open firmware projects into `experiments/` as intact,
independently buildable reference projects:

- `DinghyDisplay` is the reference for the Waveshare e-paper driver and current
  display implementation.
- `hello_wifi` is the reference for gateway Wi-Fi and weather-fetch experiments.
- `hello_wifi_participant` is the reference for participant Wi-Fi and UDP
  experiments.
- `hello_world/hello_world` is the sensor bring-up reference; place its ESP-IDF
  project contents under `experiments/hello_world/`.

Experiments are not production dependencies. In particular, production apps
must not include source files directly from an experiment directory. When a
driver or utility is ready for production, promote it into an appropriately
owned shared component, validate it there, and have production apps depend on
that component. The experiment remains useful for isolated hardware trials and
as a known reference; changes made there do not silently change production
firmware.

Transition sequence:

1. Create the monorepo skeleton and agree on the target app/component ownership
   before moving firmware code. Keep the existing repositories intact during
   this work.
2. Import the current firmware projects into `experiments/` without redesigning
   them. Preserve their project configuration and verify that each can still be
   built independently with the required ESP-IDF environment.
3. Consolidate shared code from `shared_components` into the monorepo's
   `components/` and `docs/`. Centralize nanopb and protocol sources; remove
   duplicate submodule checkouts only after all consuming projects use the
   monorepo copy.
4. Create clean production app projects under `apps/`. Promote working pieces
   incrementally; for example, extract the e-paper driver from the
   `DinghyDisplay` experiment into a display component before integrating it
   with live telemetry.
5. Implement and validate the end-to-end data path in small steps: schema,
   participant publish, gateway receive/cache, and display or dashboard
   consumption. Add Zenoh, weather, and NMEA 2000 integration as their own
   validated steps rather than importing all experimental behavior at once.
6. After the monorepo builds and the replacement apps pass their smoke tests,
   designate it as the source of truth. Archive or make the old repositories
   read-only, and update workspace files and documentation so there is no
   ambiguity about where active changes belong.

The transition is complete when each production app builds independently,
shared schemas and components have one maintained source, no production app
depends on files under `experiments/`, and the experiments remain independently
buildable references.

## 16. Development and Testing Strategy

Implement incrementally:

1. Monorepo structure and independently buildable ESP-IDF projects (see
  Section 15), then logging in each production app.
2. nanopb schemas and generated code.
3. Two-controller Zenoh-Pico publish/subscribe test.
4. Gateway telemetry registry and health reporting.
5. Local AP and HTTP dashboard.
6. AP+STA and Android hotspot connectivity.
7. Weather API ingestion and normalization.
8. NMEA 2000 hardware interface and verified PGNs.
9. E-paper subscriber and display integration.
10. Fault injection and endurance testing.
11. Optional Raspberry Pi, mobile router and cloud integration.

Test at minimum:
- No internet
- Hotspot disconnect and reconnect
- Sensor disconnected or invalid
- Controller reboot
- Gateway reboot
- Stale and out-of-order messages
- CAN bus disconnected and bus errors
- Dashboard access during telemetry load
- Invalid API responses
- Queue saturation and memory pressure

## 17. Implementation Rules for GitHub Copilot

Before modifying code:
- Inspect the existing repository and ESP-IDF configuration.
- Preserve working drivers and established interfaces.
- Check the installed ESP-IDF and component versions.
- Identify hardware capabilities rather than assuming them.
- Consult the selected protocol implementation and official specifications.

When implementing a feature:
- Prefer small, testable modules with explicit interfaces.
- Separate generated code from handwritten code.
- Avoid dynamic allocation in high-frequency paths where practical.
- Document task priorities, stack sizes and ownership.
- Handle all errors explicitly.
- Add unit tests for serialization, scaling, validation and conversion.
- Add hardware integration tests where required.
- Update documentation when schemas, topics, PGNs or interfaces change.

Do not implement all subsystems in a single source file.
Do not replace ESP-IDF with Arduino.
Do not introduce NMEA 0183 as a substitute for NMEA 2000.
Do not add mandatory cloud dependencies.
Do not invent hardware features, PGN definitions or API responses.

For each implementation step, first explain the intended changes,
affected files, hardware dependencies and validation plan.
