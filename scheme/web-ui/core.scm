;;; High-level Scheme API for starter-web-ui widgets.
(define-module (web-ui core)
  #:use-module (ice-9 optargs)
  #:use-module (ice-9 format)
  #:export (widget-type-register!
            widget-field
            widget-ref))

(define (widget-ref widget key)
  (assq-ref widget key))

(define (widget-field widget key)
  (assq-ref widget key))

(define (field-opt opts key)
  ;; Support both (type . string) and (type string) list style.
  (let ((v (assq-ref opts key)))
    (cond ((not v) #f)
          ((or (symbol? v) (string? v) (number? v) (boolean? v)) v)
          ((and (pair? v) (null? (cdr v))) (car v))
          ((pair? v) (car v))
          (else v))))

(define (opt->string v)
  (cond ((symbol? v) (symbol->string v))
        ((string? v) v)
        ((number? v) (number->string v))
        ((boolean? v) (if v "true" "false"))
        (else "")))

(define* (widget-type-register! #:key type label (tags '()) (fields '())
                                (prepare #f) (draw #f))
  (unless (and type draw)
    (error "widget-type-register! requires #:type and #:draw"))
  (let* ((fields-json
          (with-output-to-string
            (lambda ()
              (display "[")
              (let loop ((fs fields) (first? #t))
                (unless (null? fs)
                  (unless first? (display ","))
                  (let* ((f (car fs))
                         (name (car f))
                         (opts (cdr f)))
                    (display "{")
                    (format #t "\"name\":~s" (symbol->string name))
                    (let ((ty (field-opt opts 'type)))
                      (when ty
                        (format #t ",\"type\":~s" (opt->string ty))))
                    (let ((lab (field-opt opts 'label)))
                      (when lab
                        (format #t ",\"label\":~s" (opt->string lab))))
                    (let ((default (field-opt opts 'default)))
                      (when default
                        (format #t ",\"default\":~s" (opt->string default))))
                    (display "}")
                    (loop (cdr fs) #f))))
              (display "]"))))
         (prep (or prepare (lambda (w) "")))
         (draw-wrap
          (lambda (widget raw-json)
            (let ((view (draw widget raw-json)))
              (if (string? view)
                  view
                  (error "draw must return a JSON string"))))))
    (%register-widget-type! type
                            (or label (symbol->string type))
                            tags
                            fields-json
                            prep
                            draw-wrap)))
