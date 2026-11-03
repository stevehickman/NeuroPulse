## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-HUB-001 Rev {NP-FW-HUB-001 from → to}: `OI-FWHUB-28` raised, the app's stop and pause have no latency bound.** The hub's part of the stop path is derived from the code (the next heartbeat, at most 200 ms, plus the safety MCU's iteration); no document names a hazard that a bound would serve, so none is written. A decision request is drafted for Risk. No requirement changed.
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **`OI-FWHUB-28` raised (§13): the app's stop and pause have no latency bound.** After #755 the run loop acts on a stop when it is posted, so the hub's contribution is the next 200 ms heartbeat plus the safety MCU's loop iteration; the transport and the MCU's iteration are not measured here. A figure needs a hazard it traces to (`CLAUDE.md` §18); the request to Risk is `docs/human-input/oi-fwhub-28-app-stop-latency.md`. No requirement changed and nothing was built.
