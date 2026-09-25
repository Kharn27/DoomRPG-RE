# Native ordinary monster attack resolution — real CYD PASS

This milestone extends the previously hardware-proven monster attack visual owner
into the ordinary attack-sequence contract needed by current Entrance gameplay.

## Git / hardware boundary

```text
main used for rebase = b77513309a38a970a5d59195ce76424a3f44a7cb
branch = agent/esp32-native-monster-turn-ordinary-completion
rebased code boundary = e87097d7544ea63104049003c55e19a7158bfad3
hardware-tested code boundary = aa32270adbb22de6666c3ad45c5d63c88fc34db4
CI = esp32-cyd #757 SUCCESS
static RAM = 45096 B
flash = 782081 B
status = REAL-CYD PASS
```

The branch contains the earlier generic multi-loop presentation work and the
later correction that resolves retaliation only after presentation.

## Contract

The monster-turn probe remains transactional and side-effect-free. Presentation
then owns a bounded attack lease. During that lease:

```text
gameplay RNG mutation = none
player HP/armor mutation = none
world input = blocked
immutable BSP sprite = unchanged
temporary fixed animation = owned by presentation layer
```

Only after the animation reaches COMPLETE may the retaliation owner commit the
already-probed gameplay result.

This fixes the earlier mismatch where the red flash/message and HP change could
occur on the first attack frame while a multi-shot monster was still visibly
attacking and the player could move during the remaining animation.

## Real-CYD witness on current rebased head

Zombie Pvt sprite 315 exercised the alternate ordinary attack:

```text
[MONSTERTURN] ATTACK-PROBE ... sprite=315 subtype=0 ... alt=1 loops=1 ...
[MONSTERATKVIS] ARM ... visual=5 ... phaseMs=500 ... gameplayMutation=no
[MONSTERRETAL] WAIT ... resolution=after-animation playerMutation=no rngConsumed=0 worldInput=blocked
[MONSTERATKVIS] COMPLETE ... visual=5->idle ... resolution=unblocked-after-animation
[MONSTERRETAL] COMMIT ... playerHP=23->19 armor=11->7 ... attackVisual=complete-before-resolution
```

The following ordinary input is admitted only after the commit.

## Post-review correctness fixes — build-valid, hardware retest pending

Two later reviews found narrow handshake/timing gaps not exercised by the
successful hardware witness above.

### First-frame rollback retains the probe

Previously, the visual owner advanced `observedAttackProbes` before attempting
the first `guardedRender()`. If that render failed transiently, `clearSequence()`
removed the visual but retaliation continued waiting for the now-unreachable
completion. World input could reopen because no sequence remained busy.

The observed probe now advances only after successful physical presentation.
On rollback it remains pending and is retried by the next service pass;
`EspNativeGameplayMonsterAttackVisual_isBusy()` also treats this one-probe gap
as combat ownership, so neither input nor a later attack can bypass it.

### Final idle owns its full cadence

Previously, the final attack-to-idle redraw published `completedProbe`
immediately. Intermediate idle poses waited `phaseMs`, but the last one allowed
retaliation damage and input 200–500 ms early depending on subtype.

The final idle is now a distinct timed phase. It is drawn once, remains busy for
the subtype's recovered `phaseMs`, and only then publishes completion without a
redundant redraw. This matches the legacy final `animEndTime` wait before
`Combat_monsterSeq()` stage 2 / `Player_pain`.

Local validation:

```text
esp32-cyd build = SUCCESS
static RAM = 45096 B
flash = 782505 B
hardware status = retest pending for these two review fixes
```

## Multi-loop presentation boundary

Before this rebase, the same implementation was exercised on an Entrance Troop
with three repeated attack/idle phases and the user visually confirmed all three
attacks. The sequence was functionally correct but perceived as too slow.

That perceived slowness is intentionally not "fixed" here by changing only the
attack-animation delay. Current logs show broader end-to-end gameplay frame
latency, so timing optimization belongs to a separate system-level performance
milestone.

## Still deferred

```text
projectile flight visuals
attack sound playback
pain face / shake
player lethal/death transition
special subtype-10 AI
global performance/cadence optimization
```
