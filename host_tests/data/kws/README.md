# Speech samples of the keyword tests

The 15 WAV files here (16 kHz, 16 bit, mono) are **synthetic speech**: no person's voice is in them. They are made by
[`scripts/generate_kws_test_audio.ps1`](../../../scripts/generate_kws_test_audio.ps1) with the two text-to-speech voices that
ship with Windows - "Microsoft Hedda Desktop" (German) and "Microsoft Zira Desktop" (English) - and trimmed by
`scripts/trim_kws_audio.py`. They are committed so CI needs no speech engine.

- `stopp_hedda_r*.wav`: the stop word "Stopp" at four speaking speeds (`r-2` slowest ... `r3` fastest);
  `stopp_zira_r0.wav`: the English voice saying "Stop" (another speaker, another accent).
- `neg_*.wav`: words that must not trigger - unrelated ones and similar-sounding ones ("Stock", "Spott", "Stoppuhr").

**Licence.** This repository does not claim a licence for these recordings beyond what Microsoft's terms for its voices allow
for the audio they produce; the terms were not checked. If that matters for your use of the repository, regenerate the set
with a speech engine whose output you may redistribute. A trial with the open-source eSpeak engine did **not** work as a
drop-in: its voice renders "Stopp" and "Stock" almost alike, so the tests that require "Stock" to be rejected
(`KwsMatcher.*`, `KwsRobustness.*`, `KwsStream.*`) fail - the sample set and the tests belong together.
