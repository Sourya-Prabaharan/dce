/* ========================================================
   Distributed Compute Engine — App JS
   Scroll animations, nav behavior, code tabs, mobile menu
   ======================================================== */

(function () {
  "use strict";

  // ── Scroll-triggered reveal ────────────────────────────
  const revealEls = document.querySelectorAll(
    ".feature-card, .protocol-card, .tech-card, .learning-item, .arch-node"
  );

  const revealObserver = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add("visible");
          revealObserver.unobserve(entry.target);
        }
      });
    },
    { threshold: 0.15, rootMargin: "0px 0px -40px 0px" }
  );

  revealEls.forEach((el) => revealObserver.observe(el));

  // ── Code tab switching ─────────────────────────────────
  const tabs   = document.querySelectorAll(".code-tab");
  const panels = document.querySelectorAll(".code-panel");

  tabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      const target = tab.dataset.tab;
      tabs.forEach((t) => t.classList.remove("active"));
      panels.forEach((p) => p.classList.remove("active"));
      tab.classList.add("active");
      const panel = document.getElementById(`panel-${target}`);
      if (panel) panel.classList.add("active");
    });
  });

  // ── Navbar scroll effect ───────────────────────────────
  const nav = document.getElementById("main-nav");
  let lastScroll = 0;

  window.addEventListener("scroll", () => {
    const scrollY = window.scrollY;
    if (scrollY > 80) {
      nav.style.background = "rgba(10, 14, 23, 0.95)";
      nav.style.boxShadow  = "0 2px 20px rgba(0,0,0,0.3)";
    } else {
      nav.style.background = "rgba(10, 14, 23, 0.8)";
      nav.style.boxShadow  = "none";
    }
    lastScroll = scrollY;
  });

  // ── Active nav link highlighting ───────────────────────
  const sections  = document.querySelectorAll("section[id]");
  const navLinks  = document.querySelectorAll(".nav-link[href^='#']");

  const linkObserver = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          const id = entry.target.id;
          navLinks.forEach((link) => {
            link.style.color = link.getAttribute("href") === `#${id}`
              ? "var(--accent-cyan)"
              : "";
          });
        }
      });
    },
    { threshold: 0.3 }
  );

  sections.forEach((s) => linkObserver.observe(s));

  // ── Mobile menu toggle ─────────────────────────────────
  const menuBtn  = document.getElementById("mobile-menu-btn");
  const navLinksContainer = document.querySelector(".nav-links");
  let menuOpen = false;

  if (menuBtn) {
    menuBtn.addEventListener("click", () => {
      menuOpen = !menuOpen;
      if (menuOpen) {
        navLinksContainer.style.display = "flex";
        navLinksContainer.style.flexDirection = "column";
        navLinksContainer.style.position = "absolute";
        navLinksContainer.style.top = "var(--nav-height)";
        navLinksContainer.style.left = "0";
        navLinksContainer.style.right = "0";
        navLinksContainer.style.background = "rgba(10, 14, 23, 0.98)";
        navLinksContainer.style.padding = "20px 24px";
        navLinksContainer.style.borderBottom = "1px solid var(--border-color)";
        navLinksContainer.style.gap = "16px";
        menuBtn.children[0].style.transform = "rotate(45deg) translate(5px, 5px)";
        menuBtn.children[1].style.opacity = "0";
        menuBtn.children[2].style.transform = "rotate(-45deg) translate(5px, -5px)";
      } else {
        navLinksContainer.style = "";
        menuBtn.children[0].style.transform = "";
        menuBtn.children[1].style.opacity = "";
        menuBtn.children[2].style.transform = "";
      }
    });

    // Close on link click
    navLinksContainer.querySelectorAll("a").forEach((link) => {
      link.addEventListener("click", () => {
        if (menuOpen) {
          menuOpen = false;
          navLinksContainer.style = "";
          menuBtn.children[0].style.transform = "";
          menuBtn.children[1].style.opacity = "";
          menuBtn.children[2].style.transform = "";
        }
      });
    });
  }

  // ── Smooth scroll for anchor links ─────────────────────
  document.querySelectorAll('a[href^="#"]').forEach((anchor) => {
    anchor.addEventListener("click", (e) => {
      const href = anchor.getAttribute("href");
      if (href === "#") return;
      const target = document.querySelector(href);
      if (target) {
        e.preventDefault();
        const navH = parseInt(getComputedStyle(document.documentElement).getPropertyValue("--nav-height")) || 64;
        const top = target.getBoundingClientRect().top + window.scrollY - navH - 16;
        window.scrollTo({ top, behavior: "smooth" });
      }
    });
  });
})();
