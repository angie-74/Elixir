(function () {
  var items = document.querySelectorAll("[data-reveal]");
  var notice = document.querySelector(".notice");
  var i;

  if (notice) {
    document.addEventListener("click", function (event) {
      var link = event.target.closest ? event.target.closest('a[href$="Elixir.exe"]') : null;
      if (!link) {
        return;
      }
      notice.classList.remove("again");
      void notice.offsetWidth;
      notice.classList.add("again");
    });
  }

  var calm = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var questions = document.querySelectorAll(".answers details");
  var ease = "cubic-bezier(0.2, 0.7, 0.2, 1)";

  function closeQuestion(question) {
    var answer = question.querySelector(".answer");
    if (!question.open || question.classList.contains("closing")) {
      return;
    }
    if (question.motion) {
      question.motion.cancel();
    }
    question.classList.add("closing");
    question.motion = answer.animate(
      [
        { height: answer.offsetHeight + "px", opacity: 1 },
        { height: "0px", opacity: 0 }
      ],
      { duration: 280, easing: ease }
    );
    question.motion.onfinish = function () {
      question.open = false;
      question.classList.remove("closing");
      question.motion = null;
    };
  }

  function openQuestion(question) {
    var answer = question.querySelector(".answer");
    var text = answer.firstElementChild;
    if (question.motion) {
      question.motion.cancel();
    }
    question.classList.remove("closing");
    question.open = true;
    question.motion = answer.animate(
      [
        { height: "0px", opacity: 0 },
        { height: answer.scrollHeight + "px", opacity: 1 }
      ],
      { duration: 360, easing: ease }
    );
    question.motion.onfinish = function () {
      question.motion = null;
    };
    if (text && text.animate) {
      text.animate(
        [
          { transform: "translateY(-8px)" },
          { transform: "none" }
        ],
        { duration: 360, easing: ease }
      );
    }
  }

  if (!calm && document.body.animate) {
    for (i = 0; i < questions.length; i += 1) {
      (function (question) {
        var summary = question.querySelector("summary");
        summary.addEventListener("click", function (event) {
          var j;
          event.preventDefault();
          if (question.open && !question.classList.contains("closing")) {
            closeQuestion(question);
            return;
          }
          for (j = 0; j < questions.length; j += 1) {
            if (questions[j] !== question) {
              closeQuestion(questions[j]);
            }
          }
          openQuestion(question);
        });
      })(questions[i]);
    }
  }

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
