date: 2026-11-22
## DHF
author: NeurOne Software Engineering
text: **{NP-APP-PRESENCE-001 from → to}: new `REQ-PRES-10`, the passphrase record of the presence gate is sealed under the device keystore key as well as the passphrase, so a copied host store cannot be guessed at offline (`OI-PRIV-65` (1), Finding M12 of `NP-PRIV-ANALYSIS-007`).**
## NP-APP-PRESENCE-001
author: NeurOne Software Engineering
text: **New `REQ-PRES-10`: the passphrase record (Argon2 parameters, salt, wrapped data key) is written only sealed under the host keystore key the device tier uses, stays readable before presence and outside the UHDR tier, and a record in the clear is refused (`OI-PRIV-65` (1)). Increment 5 notes the change.**
