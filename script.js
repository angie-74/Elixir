(function () {
  var items = document.querySelectorAll("[data-reveal]");
  var i;

  if (!("IntersectionObserver" in window)) {
    for (i = 0; i < items.length; i += 1) {
      items[i].classList.add("in");
    }
    return;
  }

  var observer = new IntersectionObserver(
    function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add("in");
          observer.unobserve(entry.target);
        }
      });
    },
    { rootMargin: "0px 0px -6% 0px", threshold: 0.06 }
  );

  for (i = 0; i < items.length; i += 1) {
    observer.observe(items[i]);
  }
})();
