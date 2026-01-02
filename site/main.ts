const codeExamples: Record<string, string> = {
  declarations: `<span class="comment">&lt;!-- Define variables at the top of your .vyu file --&gt;</span>
<span class="expr">{{</span>
  <span class="keyword">const</span> title = <span class="string">'My Website'</span>;
  <span class="keyword">const</span> author = <span class="string">'Jane Doe'</span>;
  <span class="keyword">const</span> year = <span class="expr">new Date().getFullYear()</span>;
<span class="expr">}}</span>

<span class="tag">&lt;h1&gt;</span><span class="expr">{{ title }}</span><span class="tag">&lt;/h1&gt;</span>
<span class="tag">&lt;p&gt;</span>© <span class="expr">{{ year }}</span> <span class="expr">{{ author }}</span><span class="tag">&lt;/p&gt;</span>`,

  counter: `<span class="expr">{{</span>
  <span class="keyword">function</span> setupCounter() {
    <span class="keyword">const</span> count = document.getElementById(<span class="string">'count'</span>);
    <span class="keyword">const</span> btn = document.getElementById(<span class="string">'btn'</span>);
    <span class="keyword">let</span> value = 0;
    
    btn.addEventListener(<span class="string">'click'</span>, () => {
      value++;
      count.textContent = value.toString();
    });
  }
<span class="expr">}}</span>

<span class="tag">&lt;div&gt;</span>
  <span class="tag">&lt;span</span> <span class="attr">id</span>=<span class="string">"count"</span><span class="tag">&gt;</span>0<span class="tag">&lt;/span&gt;</span>
  <span class="tag">&lt;button</span> <span class="attr">id</span>=<span class="string">"btn"</span><span class="tag">&gt;</span>+1<span class="tag">&lt;/button&gt;</span>
<span class="tag">&lt;/div&gt;</span>

<span class="tag">&lt;script&gt;</span><span class="expr">setupCounter();</span><span class="tag">&lt;/script&gt;</span>`,

  loops: `<span class="expr">{{</span>
  <span class="keyword">const</span> items = [
    { name: <span class="string">'Apple'</span>, emoji: <span class="string">'🍎'</span> },
    { name: <span class="string">'Banana'</span>, emoji: <span class="string">'🍌'</span> },
    { name: <span class="string">'Cherry'</span>, emoji: <span class="string">'🍒'</span> }
  ];
<span class="expr">}}</span>

<span class="tag">&lt;ul&gt;</span>
  <span class="expr">{{ items.map(i => \`
    &lt;li&gt;\${i.emoji} \${i.name}&lt;/li&gt;
  \`).join('') }}</span>
<span class="tag">&lt;/ul&gt;</span>`,

  darkmode: `<span class="expr">{{</span>
  <span class="keyword">function</span> toggleTheme() {
    document.body.classList.toggle(<span class="string">'dark'</span>);
    updateTheme();
  }
  
  <span class="keyword">function</span> updateTheme() {
     <span class="comment">// Optional: Update UI based on theme</span>
  }
<span class="expr">}}</span>

<span class="tag">&lt;button</span> <span class="attr">onclick</span>=<span class="string">"toggleTheme()"</span><span class="tag">&gt;</span>
  Toggle Dark Mode
<span class="tag">&lt;/button&gt;</span>

<span class="tag">&lt;style&gt;</span>
  <span class="tag">body.dark</span> {
    <span class="attr">background</span>: <span class="string">#1a1a1a</span>;
    <span class="attr">color</span>: <span class="string">#fff</span>;
  }
<span class="tag">&lt;/style&gt;</span>`,
};

const tabsWrapper = document.querySelector(".tabs-wrapper") as HTMLElement;
const mainTabs = tabsWrapper?.querySelector(".tabs:not(.tabs-overlay)");
const overlayTabs = tabsWrapper?.querySelector(".tabs-overlay") as HTMLElement;
const allTabs = mainTabs?.querySelectorAll(".tab");
const codeDisplay = document.getElementById("code-display");
const codePanel = document.querySelector(".code-panel") as HTMLElement;
const copyBtn = document.getElementById("copy-code-btn");

function updateClipPath(activeTab: HTMLElement): void {
  if (!overlayTabs || !tabsWrapper) return;

  const wrapperRect = tabsWrapper.getBoundingClientRect();
  const tabRect = activeTab.getBoundingClientRect();

  const clipX = tabRect.left - wrapperRect.left - 4;
  const clipWidth = tabRect.width;

  overlayTabs.style.setProperty("--clip-x", `${clipX}px`);
  overlayTabs.style.setProperty("--clip-width", `${clipX + clipWidth}px`);
}

function updateCodeDisplay(tabValue: string): void {
  if (!codeDisplay || !codePanel) return;

  // 1. Lock the current height
  const startHeight = codePanel.offsetHeight;
  codePanel.style.height = `${startHeight}px`;

  // 2. Update content
  codeDisplay.innerHTML = codeExamples[tabValue] || "";

  // 3. Measure new height
  codePanel.style.height = "auto";
  const endHeight = codePanel.offsetHeight;

  // 4. Restore start height immediately
  codePanel.style.height = `${startHeight}px`;

  // 5. Force reflow
  codePanel.offsetHeight;

  // 6. Animate to end height
  codePanel.style.height = `${endHeight}px`;

  // 7. Reset to auto after transition
  const onTransitionEnd = () => {
    codePanel.style.height = "auto";
    codePanel.removeEventListener("transitionend", onTransitionEnd);
  };
  codePanel.addEventListener("transitionend", onTransitionEnd);
}

allTabs?.forEach((tab) => {
  tab.addEventListener("click", () => {
    allTabs.forEach((t) => {
      t.classList.remove("active");
    });
    overlayTabs?.querySelectorAll(".tab").forEach((t) => {
      t.classList.remove("active");
    });

    tab.classList.add("active");
    const tabValue = (tab as HTMLElement).dataset.tab || "";
    overlayTabs
      ?.querySelector(`[data-tab="${tabValue}"]`)
      ?.classList.add("active");

    updateClipPath(tab as HTMLElement);
    updateCodeDisplay(tabValue);
  });
});

// Copy Button Logic
if (copyBtn && codeDisplay) {
  copyBtn.addEventListener("click", () => {
    const text = codeDisplay.textContent || "";
    navigator.clipboard
      .writeText(text)
      .then(() => {
        const icon = copyBtn.querySelector("i");
        if (icon) {
          icon.className = "ti ti-check";
          copyBtn.classList.add("copied");

          setTimeout(() => {
            icon.className = "ti ti-copy";
            copyBtn.classList.remove("copied");
          }, 2000);
        }
      })
      .catch((err) => {
        console.error("Failed to copy:", err);
      });
  });
}

const initialActive = mainTabs?.querySelector(".tab.active") as HTMLElement;
if (initialActive) {
  const initialTab = initialActive.dataset.tab || "counter";
  if (codeDisplay) codeDisplay.innerHTML = codeExamples[initialTab] || "";

  if (overlayTabs) {
    overlayTabs.style.transition = "none";
    requestAnimationFrame(() => {
      updateClipPath(initialActive);
      requestAnimationFrame(() => {
        overlayTabs.style.transition = "";
      });
    });
  }
}

// Install Block Copy Logic
const installBlock = document.querySelector(".install-block");
if (installBlock) {
  installBlock.addEventListener("click", () => {
    const cmdSpan = installBlock.querySelector("span");
    const text = cmdSpan?.textContent?.trim() || "";

    if (text) {
      navigator.clipboard
        .writeText(text)
        .then(() => {
          const icon = installBlock.querySelector(".copy-icon i");
          if (icon) {
            icon.className = "ti ti-check";

            setTimeout(() => {
              icon.className = "ti ti-copy";
            }, 2000);
          }
        })
        .catch((err) => {
          console.error("Failed to copy install command:", err);
        });
    }
  });
}

console.log("🌬️ Vaayu site loaded!");
