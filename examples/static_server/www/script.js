// TinyC HTTP Server Demo Script
console.log("TinyC HTTP Server is serving JavaScript files correctly!");

document.addEventListener("DOMContentLoaded", function () {
  const timestamp = new Date().toISOString();
  console.log("Page loaded at:", timestamp);

  // Add a simple interactive element
  document.body.addEventListener("click", function (e) {
    if (e.target.tagName === "A") {
      console.log("Navigating to:", e.target.href);
    }
  });
});
