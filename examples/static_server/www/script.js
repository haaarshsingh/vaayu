console.log("Server serving JS file");

document.addEventListener("DOMContentLoaded", function () {
  const timestamp = new Date().toISOString();
  console.log("Page loaded at:", timestamp);

  document.body.addEventListener("click", function (e) {
    if (e.target.tagName === "A") {
      console.log("Navigating to:", e.target.href);
    }
  });
});
