date: 2026-11-21
## DHF
author: NeurOne Software Engineering
text: **{NP-INFRA-001 from → to}, {NP-ANALYTICS-001 from → to}, {NP-APP-TELEMETRY-001 from → to}, {NP-PRIV-ANALYSIS-005 from → to}: `OI-PRIV-51` and `OI-PRIV-52` closed. Both Caddy edges delete every request header from their logs and strip the full client-address header set upstream; every SHDR month anchor is month-granular by CHECK (schema Rev F, `REQ-SHDR-11`), and the gate refuses interval and clock-named columns.**
## NP-INFRA-001
author: NeurOne Software Engineering
text: **§3.3a: the fleet edge's logs delete every request header, and the edge removes nine client-address headers before the ingest service, not two (`OI-PRIV-51`, `REQ-SHDR-06`).**
## NP-ANALYTICS-001
author: NeurOne Software Engineering
text: **The network-address paragraph: the analytics proxy removes nine client-address headers before PostHog and the erasure relay, and its logs keep no request header (`OI-PRIV-51`).**
## NP-APP-TELEMETRY-001
author: NeurOne Software Engineering
text: **§4.1 (the floor): the analytics proxy removes every header a proxy can carry the address in, before PostHog or the erasure relay, and neither log keeps any request header (`OI-PRIV-51`).**
## NP-PRIV-ANALYSIS-005
author: NeurOne Software Engineering
text: **F1 and F2 marked fixed with what remains; F2's count corrected from 12 of 21 to 13 of 22 (`emf_shielding_telemetry`, found by the new TIME-02 check); `OI-PRIV-51` and `-52` closed in §8.**
