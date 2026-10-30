---
title: 'Spring dispersion and allpass fitting'
updated: 2026-10-29
confidence: medium-high
---

# Spring dispersion, and the curve we fit the allpass cascade to

[SPEC-001](../../specs/active/SPEC-001-spring-tank-dsp-core.md) could not reach `ready`
until one decision was made: **which published dispersion curve does the allpass cascade
get fitted to, and with what coefficients.** This document is that decision, its
derivation, and its validation.

It also corrects something the earlier documents got backwards. See
[The chirp ascends](#the-chirp-ascends-and-the-descending-one-is-the-quiet-one).

## The decision

> Fit the group delay of a **cascade of stretched first-order allpass sections** to the
> closed-form group-delay expression in **Parker (2011), eq. (4)**, with the physical
> targets `F_C` (transition frequency) and `T_D` (echo spacing) computed from
> **Bilbao & Parker (2009), eqs. (2)-(4)**.

### Why these two papers and not the other two

Four papers could have supplied the curve. The deciding factor was not accuracy — it was
readability by contributors.

| Source | Gives us | Available? |
| --- | --- | --- |
| **Parker, EURASIP 2011** | Closed-form group delay of the stretched cascade; the structure; published section counts | **Open access, CC BY** |
| **Bilbao & Parker, DAFx-09** | Closed forms for `F_C` and `T_D` from spring geometry, plus calipered measurements of five real springs | **Free from the DAFx archive** |
| Valimaki, Parker & Abel, JAES 2010 | The original parametric model | Paywalled (JAES) |
| Abel, Berners, Costello & Smith, AES 2006 | The original dispersive-waveguide model | Paywalled (AES preprint 6954) |

birbverb's whole claim is that every audible constant traces to a source a reader can
check. A constant sourced to a paper nobody can open is a constant on trust, which is the
thing this project exists not to do. Parker 2011 and Bilbao & Parker 2009 between them
give the full chain — physical geometry, then `F_C` and `T_D`, then allpass coefficients —
and both are free. The two paywalled papers remain the primary literature and are cited
as such; we just do not depend on them for a number.

## The physics

A helical spring carries a wave whose speed depends on frequency. Bilbao and Parker take
the Fletcher model (a curved rod — the helix angle is approximated as zero), write it as a
dispersion relation in wavenumber `beta` and angular frequency `omega`, and solve it. Two
mode pairs come out; one is above hearing. The audible one has a **local maximum in
frequency versus wavenumber**, and that maximum is the transition frequency `F_C`.

Three consequences, all of which the model has to reproduce:

1. **Below `F_C` the relation is not single-valued.** A given frequency below `F_C` has
   *three* propagation velocities. Hardware carries three simultaneous arrivals of the
   same frequency.
2. **`F_C` is the point of minimum propagation velocity**, so group delay *peaks* there.
3. **There is a second, much quieter echo series** spanning the whole audio band, with
   maximum group delay at DC falling as frequency rises. Parker puts it "generally at
   least 30 dB below" the main series.

### The chirp ascends, and the descending one is the quiet one

Earlier birbverb documents — SPEC-001 requirement 1, acceptance criterion 1, ADR-001's
Context, and the tank study — all described the spring's impulse response as a
**descending** chirp. That is wrong for the series that matters.

Below `F_C`, group delay *increases* with frequency and peaks at `F_C`. Low frequencies
travel fastest and arrive first. So each echo in the dominant series is an **ascending**
chirp:

| Series | Band | Group delay | Chirp | Level |
| --- | --- | --- | --- | --- |
| Main (`C_lf`) | below `F_C` | minimum at DC, **maximum at `F_C`** | **ascending** | reference |
| Secondary (`C_hf`) | full band | **maximum at DC**, falling with frequency | descending | 30 dB or more down |

Both are real and birbverb models both, but if only one were implemented it would have to
be the ascending one. A model that produced only a descending chirp would be modelling the
30-dB-down artefact and discarding the sound of the instrument.

This is the kind of error document-driven development is supposed to catch before code,
and it did: it was caught while recording a curve, with no allpass written yet.

### Closed forms

With `E` Young's modulus, `rho` density, `L` the uncurled wire length, `R` the helix
radius, `r` the wire radius, `H_L` the helix length and `N` the number of turns:

```
L   = sqrt( (2*pi*R*N)^2 + H_L^2 )                      [B&P eq. 3]

T_D ~= 4*L*R / ( r * sqrt(E/rho) )                      [B&P eq. 2]

F_C ~= 3*r*sqrt(E/rho) / ( 16*sqrt(5)*pi*R^2 )          [B&P eq. 4]
```

`T_D` is the time for a wave to traverse the spring **twice** — the spacing between
echoes. Single-pass delay is `T_D / 2`.

For steel, Bilbao and Parker use `E = 2e11 N/m^2` and `rho = 7800 kg/m^3`, giving a bar
wave speed of **5063.7 m/s**.

### `F_C` does not depend on spring length

Read the two expressions together:

```
T_D  proportional to  L*R / r          F_C  proportional to  r / R^2
```

**`F_C` has no `L` in it at all.** Echo spacing is set by length and helix radius; the
transition frequency is set by wire and helix radius only. A designer can change one
without the other, and the measurements show manufacturers did exactly that:

- The **Olson X-82** (2 springs) has near-matched `T_D` and widely spread `F_C`.
- The **Leem KA-1210** (3 springs) has spread `T_D` and near-matched `F_C`.

Bilbao and Parker state both design philosophies in prose. Recomputing from the geometry
alone recovers them, which is a useful check that the formulas were transcribed correctly.

## Validation

Both units' geometries were calipered by Bilbao and Parker and published in their
Appendix A. Feeding those numbers through eqs. (2)-(4):

| Spring | `H_L` | helix dia. | turns | wire dia. | `T_D` | 1-pass | `F_C` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Olson X-82 #1 | 6.5 cm | 0.54 cm | 148 | 0.035 cm | 30.61 ms | 15.31 ms | 3244 Hz |
| Olson X-82 #2 | 6.5 cm | 0.61 cm | 133 | 0.035 cm | 35.10 ms | 17.55 ms | 2543 Hz |
| Leem KA-1210 #1 | 16.3 cm | 0.44 cm | 303 | 0.035 cm | 41.62 ms | 20.81 ms | 4887 Hz |
| Leem KA-1210 #2 | 16.3 cm | 0.45 cm | 280 | 0.035 cm | 40.24 ms | 20.12 ms | 4672 Hz |
| Leem KA-1210 #3 | 16.3 cm | 0.46 cm | 351 | 0.035 cm | 52.69 ms | 26.34 ms | 4471 Hz |

| Unit | `T_D` spread | `F_C` spread | Matches the stated philosophy? |
| --- | --- | --- | --- |
| Olson X-82, 2 springs | 13.7% | 24.3% | yes — matched delay, spread transition |
| Leem KA-1210, 3 springs | 27.8% | 8.9% | yes — spread delay, matched transition |

Three independent cross-checks pass:

1. **Every computed `F_C` lands between 2.5 and 4.9 kHz.** Parker, in a different paper,
   says an `F_C` above 5 kHz "is rarely seen in real spring reverberators" — a statement
   made without reference to these geometries. The formula and the remark agree.
2. **Computed `T_D` spans 30-53 ms.** Commercial tank literature quotes early-reflection
   delays of roughly 30-40 ms, with long tanks longer. Agrees.
3. **Both design philosophies fall out of the geometry**, as above.

### And one check against our own test harness

Parker's group-delay expression for the cascade is

```
D(omega) = k*M*(1 - a^2) / ( 1 + 2*a*cos(omega*k) + a^2 )       [Parker eq. 4]
```

with `k` the stretch factor, `M` the number of cascaded sections, `a` the allpass
coefficient. At `a = -0.7, k = 1, M = 1` it gives **5.6667 samples** at DC.

While verifying `tests/support/Measure.h` (see
[SPEC-006](../../specs/active/SPEC-006-offline-dsp-test-harness.md)), `groupDelay()`
measured a single `a = -0.7` allpass at **5.6525 samples** at the lowest non-DC bin —
0.25% from the closed form, the difference being that the measurement is taken at the
first bin rather than exactly at DC.

So the paper's formula and birbverb's measurement code independently agree. That is worth
more than either on its own: the formula is transcribed correctly, *and* the group-delay
measurement that acceptance criterion 1 depends on is sound.

Parker's inverse-design expression for a fractional delay at an arbitrary frequency,

```
a = ( -D*cos(omega) +/- sqrt( 1 - D^2 + D^2*cos(omega)^2 ) ) / ( 1 + D )   [Parker eq. 6]
```

was also checked: it is the exact algebraic solution of eq. (5) for `a`, and it
round-trips to six decimal places for both roots at several `(D, omega)` pairs.

## The structure, and how the coefficients are derived

Valimaki et al.'s model, as Parker describes it, is two parallel feedback delay lines:

```
                  +----------------------------------+
         +-------->| C_lf : delay -> A^Mlow(z^k) -> LPF |-------+
in ------+        +----------------------------------+        +---> wet
         |        +----------------------------------+        |
         +-------->| C_hf : delay -> A^Mhigh(z)         |-------+  (30 dB or more down)
                  +----------------------------------+
```

Starting from the first-order allpass and stretching it:

```
A(z)        = (a + z^-1) / (1 + a*z^-1)                  [Parker eq. 1]
H_single(z) = A(z^k) = (a + z^-k) / (1 + a*z^-k)         [Parker eq. 2]
H_cascade   = A^M(z^k)                                   [Parker eq. 3]
```

Three design identities follow from eq. (4), and these are what the code uses:

```
peak of D is where cos(omega*k) = -1,  i.e.  f_peak = fs / (2*k)
  =>  k        = fs / (2 * F_C)

D(f_peak)      = k*M*(1 + a) / (1 - a)
  =>  a        = (D_peak - k*M) / (D_peak + k*M)

D(DC)          = k*M*(1 - a) / (1 + a)
```

So: `F_C` picks the stretch factor, the target chirp length picks the coefficient, and
`M` is taken from the literature. Setting `D_peak = T_D` makes the chirp just fill the
echo period, which is what the published spectrograms show and what makes a spring sound
continuous rather than like discrete taps.

`k` comes out fractional. Integer `k` would make `F_C` jump with sample rate — at 48 kHz,
`k = 5` gives 4800 Hz and `k = 6` gives 4000 Hz — which would violate SPEC-001
requirement 9. Fractional `k` is supported by replacing one internal unit delay with an
interpolating first-order allpass, designed with eq. (6). **That interpolator is not
optional**; it is what makes the model sample-rate independent.

### Section counts

`M_low = 100` and `M_high = 200`, from Parker 2011 section 4, which states these as the
values "used by Valimaki et al."

Parker's chirp-straightening method halves both (`M/2`) by discarding the low half of the
chirp, where dispersion is weak, and replacing it with a crossover and a matched delay.
birbverb does **not** do this in v0.1: it trades a documented structure for an
undocumented approximation with a known notch at the crossover, and we have no listening
reference to judge the trade against. It is recorded here as the first optimisation to
reach for if CPU becomes the binding constraint, since it is published, measured, and
reversible.

## Coefficients, per tank type

Computed at 48 kHz with `M_low = 100`, `M_high = 200`. `a_hf = -0.6` for every tank
(negative, so the secondary chirp's group delay peaks at DC): that gives 16.7 ms at DC,
2.5 ms at 10 kHz and 1.0 ms at Nyquist, matching Parker's "little dispersion above
10 kHz".

### Short 2-Spring — proxy: Olson X-82, **measured**

| Spring | `T_D` | `F_C` | `k` | `a_lf` | `D(DC)` | `D(F_C)` |
| --- | --- | --- | --- | --- | --- | --- |
| A | 30.61 ms | 3244 Hz | 7.397 | +0.33027 | 7.76 ms | 30.61 ms |
| B | 35.10 ms | 2543 Hz | 9.439 | +0.28186 | 11.02 ms | 35.10 ms |

### Long 3-Spring — proxy: Leem KA-1210, **measured**

| Spring | `T_D` | `F_C` | `k` | `a_lf` | `D(DC)` | `D(F_C)` |
| --- | --- | --- | --- | --- | --- | --- |
| A | 41.62 ms | 4887 Hz | 4.911 | +0.60539 | 2.51 ms | 41.62 ms |
| B | 40.24 ms | 4672 Hz | 5.137 | +0.57981 | 2.85 ms | 40.24 ms |
| C | 52.69 ms | 4471 Hz | 5.368 | +0.64983 | 2.37 ms | 52.69 ms |

### Long 2-Spring — **interpolated**, no measured unit exists

This is the Fender blackface tank and the most important one in the bank, and it is the
one with no published geometry. Nobody in the literature calipered an Accutronics Type 4
long pan.

It is constructed as: the Leem's long-class wire and helix radii (same manufacturing
class, a 1990s guitar amp tank), arranged the way the Olson's **two**-spring unit is —
matched `T_D`, spread `F_C` — because that is the documented 2-spring philosophy.

| Spring | `T_D` | `F_C` | `k` | `a_lf` | `D(DC)` | `D(F_C)` |
| --- | --- | --- | --- | --- | --- | --- |
| A | 52.69 ms | 4471 Hz | 5.368 | +0.64983 | 2.37 ms | 52.69 ms |
| B | 57.28 ms | 3784 Hz | 6.342 | +0.62517 | 3.05 ms | 57.28 ms |

Spread: `T_D` 8.4%, `F_C` 16.6%.

**Label this one INTERPOLATED in code and in the spec.** Its resolution path is cheap and
concrete: put a micrometer on a real `4AB3C1B`, read off helix length, helix diameter,
turns and wire diameter, and eqs. (2)-(4) give both numbers with no fitting.

### Loop gain for the decay targets

With `g = 10^(-3*T_loop / RT60)`:

| Tank | `T_loop` | Target RT60 | `g` | per pass | passes |
| --- | --- | --- | --- | --- | --- |
| Short 2-Spring | 32.86 ms | 1.6 s | 0.86775 | -1.232 dB | 49 |
| Long 2-Spring | 54.99 ms | 3.4 s | 0.89430 | -0.970 dB | 62 |
| Long 3-Spring | 44.85 ms | 3.0 s | 0.90188 | -0.897 dB | 67 |

One consequence to watch: because group delay peaks at `F_C`, energy near `F_C` makes
*fewer* round trips per second, so a flat loss filter would make `F_C` decay **slower**
than the rest of the band — the opposite of SPEC-001 requirement 7. The loss filter has
to be a low-pass with enough slope to overcome that, and the test for requirement 7 is
therefore not a formality. It is checking a real tension in the structure.

## Still unknown

| Question | Why it matters | How to close it |
| --- | --- | --- |
| Accutronics Type 4 spring geometry | The Long 2-Spring row above is interpolated, not measured | Calipers on a real tank; eqs. (2)-(4) then give `T_D` and `F_C` directly |
| Are a tank's springs in series or parallel? | Series roughly doubles `T_D`; parallel gives a comb. Both measured units are **parallel**, and birbverb models parallel | Inspect a tank, or find an Accutronics assembly drawing |
| Transducer reflection coefficient | Sets how pronounced the repeat structure is | Fit to the echo decay envelope of a measured IR |
| Loss filter shape | Requirement 7 depends on it, against the `F_C` tension above | Fit to frequency-dependent RT60 of a measured IR |
| Does the triple-valued region below `F_C` matter? | Hardware gives three arrivals per frequency; one allpass chain gives one. Parker notes this is why signal-processing models need parallel paths | Listening test against a real unit; possibly three parallel chains per spring |

The first two are answerable with a tank and a micrometer and would move this document
from `medium-high` to `high`.

## Sources

1. **J. Parker, "Efficient Dispersion Generation Structures for Spring Reverb
   Emulation", EURASIP Journal on Advances in Signal Processing, 2011, article 646134.**
   doi:10.1155/2011/646134. Open access, CC BY. — *primary.* Eqs. (1)-(6), the two-loop
   structure, `M_low = 100` / `M_high = 200`, "`F_C` above 5 kHz is rarely seen",
   "`C_hf` generally at least 30 dB below", "little dispersion above 10 kHz".
2. **S. Bilbao and J. Parker, "Spring Reverberation: A Physical Perspective", Proc.
   DAFx-09, Como, Italy, Sept. 2009, pp. 416-421.** Free from the DAFx paper archive. —
   *primary.* The dispersion relation, eqs. (2)-(4), the group-velocity limit, and the
   calipered geometries of the Olson X-82 and Leem KA-1210 in Appendix A.
3. V. Valimaki, J. Parker and J. S. Abel, "Parametric Spring Reverberation Effect",
   J. Audio Eng. Soc., vol. 58, no. 7/8, pp. 547-562, 2010. — *primary, paywalled.* The
   original model. Cited through source 1; no birbverb constant depends on it.
4. J. S. Abel, D. P. Berners, S. Costello and J. O. Smith III, "Spring Reverb Emulation
   Using Dispersive Allpass Filters in a Waveguide Structure", AES 121st Convention,
   preprint 6954, Oct. 2006. — *primary, paywalled.* The original dispersive-waveguide
   model. Cited for completeness; no birbverb constant depends on it.
5. S. Bilbao and J. Parker, "A Virtual Model of Spring Reverberation", IEEE Trans. Audio,
   Speech and Language Processing, vol. 18, no. 4, pp. 799-808, 2010. — *secondary, not
   consulted directly.* The finite-difference approach rejected in ADR-001.

Material properties for steel (`E = 2e11 N/m^2`, `rho = 7800 kg/m^3`) are from source 2's
Appendix A, which states both units appear to use steel wire.

## Notes

- Approach decision: [ADR-001](../../specs/adrs/ADR-001-spring-reverb-modelling-approach.md)
- Implementation spec: [SPEC-001](../../specs/active/SPEC-001-spring-tank-dsp-core.md)
- Approach survey: [spring-reverb-dsp-approaches](spring-reverb-dsp-approaches.md)
- Tank hardware: [accutronics-spring-tanks](../circuits/accutronics-spring-tanks.md)
