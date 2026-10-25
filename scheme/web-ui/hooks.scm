;;; starter-web-ui hooks
(define-module (web-ui hooks)
  #:export (add-hook! invoke-hook))

(define %hooks '())

(define (add-hook! name proc)
  (let ((entry (assq name %hooks)))
    (if entry
        (set-cdr! entry (cons proc (cdr entry)))
        (set! %hooks (acons name (list proc) %hooks)))))

(define (invoke-hook name . args)
  (let ((entry (assq name %hooks)))
    (when entry
      (for-each (lambda (p) (apply p args))
                (reverse (cdr entry))))))
