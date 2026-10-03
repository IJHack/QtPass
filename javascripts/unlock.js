// Easter egg: five clicks on the QtPass logo (see main.js) and the padlocked heart
// unlocks. It punches in, beats, its shackle lifts and swings open round its long
// leg, the heart beats free for a moment, then it locks again and fades away.
// The heart is the logo without its shackle (images/heart.svg); the shackle is
// drawn here from the logo's own geometry, as a round bar, so it keeps its
// thickness while it turns. Click or Esc ends it early.
(function () {
  var VIEW = 3230; // the logo's viewBox runs from (-1615, -1050) to (1615, 2180)
  var PAD_X = 0.4; // room beside the heart for the swung shackle
  var PAD_TOP = 0.3; // and above it for the lifted one
  var LENGTH = 5.6; // seconds
  var heart = new Image();
  heart.src = "/images/heart.svg";
  var running = false;

  var ease = function (a, b, t) {
    return Math.min(1, Math.max(0, (t - a) / (b - a)));
  };
  var smooth = function (u) {
    return u * u * (3 - 2 * u);
  };
  var back = function (u) {
    return 1 + 2.7 * Math.pow(u - 1, 3) + 1.7 * Math.pow(u - 1, 2);
  };
  // A heartbeat: lub-dub, every `period` seconds from `from` on.
  var beat = function (t, from, period) {
    if (t < from) {
      return 0;
    }
    var p = (t - from) % period;
    return Math.exp(-14 * p) + 0.6 * Math.exp(-14 * Math.max(0, p - 0.18)) * (p > 0.18 ? 1 : 0);
  };

  function drawPadlock(g, S, lift, swing) {
    var px = PAD_X * S;
    var py = PAD_TOP * S;
    var k = S / VIEW;
    g.save();
    // Logo units, y up, as in the logo's own transform.
    g.setTransform(k, 0, 0, -k, px + 1615 * k, py - lift + 2186 * k);
    g.save();
    // Turned round its long left leg: only the centre line is foreshortened.
    g.translate(-630, 0);
    g.scale(Math.cos(swing * Math.PI), 1);
    g.translate(630, 0);
    g.beginPath();
    g.moveTo(-630, 0);
    g.lineTo(-630, 1320);
    g.arc(0, 1320, 630, Math.PI, 0, true);
    g.lineTo(630, 900); // the short leg: just into the heart, so lifted, it's out
    g.restore();
    // The logo's shading across the bar: gray at the edges, silver in the middle. As
    // nested strokes, outside in, it looks the same from every side.
    for (var w = 360; w > 0; w -= 12) {
      var v = Math.round(128 + 64 * Math.min(1, Math.max(0, (0.45 - w / 720) / 0.35)));
      g.lineWidth = w;
      g.strokeStyle = "rgb(" + v + "," + v + "," + v + ")";
      g.stroke();
    }
    g.restore();
    g.drawImage(heart, px, py, S, S);
  }

  function smallHeart(g, x, y, r) {
    g.beginPath();
    g.moveTo(x, y + r * 0.9);
    g.bezierCurveTo(x - r * 1.4, y - r * 0.1, x - r * 0.6, y - r * 1.1, x, y - r * 0.35);
    g.bezierCurveTo(x + r * 0.6, y - r * 1.1, x + r * 1.4, y - r * 0.1, x, y + r * 0.9);
    g.fill();
  }

  function unlock() {
    if (running || !heart.complete || !heart.naturalWidth) {
      return;
    }
    running = true;
    var still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    var canvas = document.createElement("canvas");
    canvas.setAttribute("aria-hidden", "true");
    Object.assign(canvas.style, {
      position: "fixed",
      inset: "0",
      width: "100%",
      height: "100%",
      zIndex: "1000",
      cursor: "pointer",
    });
    document.body.appendChild(canvas);
    var g = canvas.getContext("2d");
    var dpr = Math.min(2, window.devicePixelRatio || 1);
    var W = window.innerWidth;
    var H = window.innerHeight;
    canvas.width = Math.round(W * dpr);
    canvas.height = Math.round(H * dpr);
    var S = Math.round(Math.min(W, H) * 0.5);
    var buf = document.createElement("canvas");
    buf.width = Math.round(S * (1 + 2 * PAD_X) * dpr);
    buf.height = Math.round(S * (1 + PAD_TOP) * dpr);
    var bg = buf.getContext("2d");
    var floaters = [];
    var t0 = performance.now();
    var last = t0;
    var raf = 0;

    function stop() {
      cancelAnimationFrame(raf);
      canvas.remove();
      window.removeEventListener("keydown", onKey);
      running = false;
    }
    function onKey(event) {
      if (event.key === "Escape") {
        stop();
      }
    }
    canvas.addEventListener("click", stop);
    window.addEventListener("keydown", onKey);

    function frame() {
      var now = performance.now();
      var t = still ? 3.2 : (now - t0) / 1000;
      var dt = Math.min(0.1, (now - last) / 1000);
      last = now;
      if (!still && t > LENGTH) {
        stop();
        return;
      }
      var appear = still ? 1 : back(ease(0, 0.45, t));
      var leave = smooth(ease(LENGTH - 0.6, LENGTH, t));
      var lift = (smooth(ease(1.7, 2.1, t)) - smooth(ease(4.7, 4.95, t))) * 0.16 * S;
      var swing = smooth(ease(2.1, 2.9, t)) * (1 - smooth(ease(4.1, 4.7, t)));
      var open = ease(2.6, 2.9, t) * (1 - ease(4.1, 4.4, t));
      var pulse = still ? 0 : beat(t, 0.6, 0.55) * (t < 1.7 ? 1 : 0) + beat(t, 2.9, 0.4) * open;
      var click = t > 4.95 && t < 5.15 ? Math.sin((t - 4.95) * 60) * 0.012 * S : 0;

      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      g.clearRect(0, 0, W, H);
      g.fillStyle = "rgba(10, 12, 24, " + 0.6 * Math.min(1, t / 0.3) * (1 - leave) + ")";
      g.fillRect(0, 0, W, H);

      // A warm glow while it's open, and little hearts drifting up out of it.
      if (open > 0) {
        var glow = g.createRadialGradient(W / 2, H / 2, 0, W / 2, H / 2, S * 0.9);
        glow.addColorStop(0, "rgba(255, 90, 140, " + 0.35 * open + ")");
        glow.addColorStop(1, "rgba(255, 90, 140, 0)");
        g.fillStyle = glow;
        g.fillRect(0, 0, W, H);
        if (!still && Math.random() < 0.5) {
          floaters.push({ x: W / 2 + (Math.random() - 0.5) * S * 0.6, y: H / 2, v: 0.25 + Math.random() * 0.3, r: S * (0.03 + Math.random() * 0.03), a: 1 });
        }
      }
      floaters.forEach(function (f) {
        f.y -= f.v * S * dt;
        f.a -= dt * 0.7;
        g.fillStyle = "rgba(240, 80, 130, " + Math.max(0, f.a) * (1 - leave) + ")";
        smallHeart(g, f.x, f.y, f.r);
      });
      floaters = floaters.filter(function (f) {
        return f.a > 0;
      });

      bg.setTransform(dpr, 0, 0, dpr, 0, 0);
      bg.clearRect(0, 0, buf.width, buf.height);
      drawPadlock(bg, S, lift, swing);
      var scale = appear * (1 + 0.1 * pulse) * (1 - 0.3 * leave);
      g.save();
      g.globalAlpha = 1 - leave;
      g.translate(W / 2 + click, H / 2 + S * 0.06);
      g.scale(scale, scale);
      // The heart's centre, not the buffer's, on the middle of the screen.
      g.drawImage(buf, -(PAD_X * S + S / 2), -(PAD_TOP * S + S / 2), S * (1 + 2 * PAD_X), S * (1 + PAD_TOP));
      g.restore();
      if (still) {
        return; // one frame; click or Esc closes it
      }
      raf = requestAnimationFrame(frame);
    }
    raf = requestAnimationFrame(frame);
  }

  window.qtpassUnlock = function () {
    if (heart.complete) {
      unlock();
    } else {
      heart.addEventListener("load", unlock, { once: true });
    }
  };
})();
