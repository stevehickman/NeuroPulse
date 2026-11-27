# Vendored third-party files of the simulator page

Everything the page (`simulator/index.html`) loads that NeurOne did not write is here, so the page asks no third
party for anything (`OI-PRIV-44`, `REQ-SIMFW-30`, `NP-SIM-FW-001` §35). The SOUP record is `NP-SOUP-SIM-001`
(`docs/np_soup_sim_001.md`). These files are committed: no build, container or CI job fetches them, and none may.

**Do not edit a file here.** To change a version, replace the files from the same source at the new version, update
the table below, the record and the importmap's hash in `index.html` together. `js/page-policy.test.mjs` fails if a
file's SHA-256 is not the one below, or if a file is here and not listed.

## Sources (fetched 2026-11-26)

| Item | Version | Source | Licence |
|------|---------|--------|---------|
| Three.js (`three`) | 0.164.1 | npm registry tarball `three-0.164.1.tgz`, sha1 `b742f76bd8dfd3736ba0d86a12dfddb73c5cdcc0` (the registry's `dist.shasum`, matched); `build/three.module.js`, `examples/jsm/controls/OrbitControls.js` and `LICENSE` copied unchanged | MIT (`three/LICENSE`) |
| Inter | 4.001 (`Version 4.001;git-66647c0bb` in each file's name table), Google Fonts `v20` | the woff2 files `fonts.googleapis.com/css2?family=Inter:wght@400;600;700` served, unchanged, renamed by subset | SIL OFL 1.1 (`fonts/OFL-Inter.txt`, from `google/fonts` `ofl/inter/OFL.txt`) |
| JetBrains Mono | 2.211 (each file's name table), Google Fonts `v24` | the woff2 files `fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600` served, unchanged, renamed by subset | SIL OFL 1.1 (`fonts/OFL-JetBrainsMono.txt`, from `google/fonts` `ofl/jetbrainsmono/OFL.txt`) |

`fonts/fonts.css` is NeurOne's: the stylesheet Google served for the request the page used to make (Chrome 124 user
agent), with each `url()` pointed at the local file. Both families are variable fonts, so one file per subset serves
every weight, and the `unicode-range` of each face keeps a browser from loading a subset the page does not show.

## Files

| File | SHA-256 |
|------|---------|
| `fonts/Inter-cyrillic-ext.woff2` | `ca157063339ac4ad418f214f3abfed119b0798ab4d377386ce5c9e5a7a435ebd` |
| `fonts/Inter-cyrillic.woff2` | `71d5ee93cc1e9f1d520a3a8b66456de18c7879d8df09d57fcd2eaff75fef0075` |
| `fonts/Inter-greek-ext.woff2` | `6e9e020a25f9b56d418f2c085b1d3c09725a4da23fe693a5b463064606732190` |
| `fonts/Inter-greek.woff2` | `1be3448e292fbf05ffe176fe1e43f135013d50b1e7d324ad1a558f623d3bb6f6` |
| `fonts/Inter-latin-ext.woff2` | `34b9c504cab7a73e37b746343a449132e56cf7b5481af2cb81dc74dcff25c956` |
| `fonts/Inter-latin.woff2` | `3100e775e8616cd2611beecfa23a4263d7037586789b43f035236a2e6fbd4c62` |
| `fonts/Inter-vietnamese.woff2` | `5c66f9e07e90c6d4ac4922cc68d60de26c17b1858e677fb5e603fce3952b3ff2` |
| `fonts/JetBrainsMono-cyrillic-ext.woff2` | `62213be8a78b42f1e29d1452d91e2f8b3e745572a9dd98d3941e39fa00b37d76` |
| `fonts/JetBrainsMono-cyrillic.woff2` | `e17cfd15fb96909d64095015f958207063a0c07191da3512df7d560a781aebdf` |
| `fonts/JetBrainsMono-greek.woff2` | `0a557721b1f8b36d3f3f84442689a71ca4a744300abcb46a1953f51bfc663b66` |
| `fonts/JetBrainsMono-latin-ext.woff2` | `db5ff4db83e580426280e9337a58dc57d3a83784a1b03ad80914651594441d52` |
| `fonts/JetBrainsMono-latin.woff2` | `83c005d49d8a6a50474c73a5a36ac0468076e9c4a29da7bdb14995d80560a5be` |
| `fonts/JetBrainsMono-vietnamese.woff2` | `c89b9cc0bc6262bd4f8d8494b6961601f3aefa829d08c2e3635f4d501d3a47c2` |
| `fonts/OFL-Inter.txt` | `5b9321a4298cfeb6b34354164a1c3afc3db114569984c502b9b35d988fd58c57` |
| `fonts/OFL-JetBrainsMono.txt` | `b2fe5e8987594e9ffd1d2ca52a2f5d73eb8335243893c5d6254b5ad69269591d` |
| `fonts/fonts.css` | `e089ef4471b6b5f59c149ae970f050d8afd572c50bba8e09411e004493edc20c` |
| `three/LICENSE` | `4c40a1ef62450b857c3b2aaf294936304cd552d965fbcd9d32d4c5bcf4ba4454` |
| `three/build/three.module.js` | `a97ec6852978949b7941c070b81292b842473a37549222353fa0a70c18d7285a` |
| `three/examples/jsm/controls/OrbitControls.js` | `f260591ef315aa04888152e7f121865214e33fb54727145cf4e4445058db1297` |
