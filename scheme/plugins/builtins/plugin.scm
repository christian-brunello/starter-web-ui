;;; Built-in widget types: toggle, slider, sensor, chart
(use-modules (web-ui core)
             (ice-9 format)
             (ice-9 regex))

(define (json-escape s)
  (let ((out (string-fold
              (lambda (ch acc)
                (string-append acc
                               (case ch
                                 ((#\\) "\\\\")
                                 ((#\") "\\\"")
                                 ((#\newline) "\\n")
                                 (else (string ch)))))
              ""
              (if (string? s) s ""))))
    out))

(define (assq-str widget key)
  (let ((v (assq-ref widget key)))
    (cond ((string? v) v)
          ((symbol? v) (symbol->string v))
          (else ""))))

(define (assq-num widget key default)
  (let* ((s (assq-str widget key))
         (n (and (not (string-null? s)) (string->number s))))
    (if (number? n) n default)))

(define (parse-val-from-raw raw)
  ;; raw is JSON object string; pull "val" number crudely
  (let ((m (string-match "\"val\"[ \t]*:[ \t]*(-?[0-9]+(\\.[0-9]+)?)" raw)))
    (if m
        (string->number (match:substring m 1))
        0)))

(widget-type-register!
 #:type 'toggle
 #:label "On/Off"
 #:tags '(digital live writable input)
 #:fields
 '((title (type string) (label "Titolo") (default ""))
   (icon (type string) (label "Icona") (default "toggle"))
   (refresh-sec (type int) (label "Refresh (s)") (default "5")))
 #:prepare (lambda (w) "")
 #:draw
 (lambda (widget raw)
   (let* ((title (let ((t (assq-str widget 'title)))
                   (if (string-null? t) (assq-str widget 'id) t)))
          (val (parse-val-from-raw raw))
          (on? (>= val 0.5)))
     (format #f
             "{\"kind\":\"toggle\",\"id\":~s,\"title\":~s,\"on\":~a}"
             (assq-str widget 'id)
             title
             (if on? "true" "false")))))

(widget-type-register!
 #:type 'slider
 #:label "Slider"
 #:tags '(analog live writable input)
 #:fields
 '((title (type string) (label "Titolo") (default ""))
   (min (type number) (label "Min") (default "0"))
   (max (type number) (label "Max") (default "100"))
   (step (type number) (label "Step") (default "0.1"))
   (show_value (type bool) (label "Mostra valore") (default "true"))
   (refresh-sec (type int) (label "Refresh (s)") (default "5")))
 #:prepare (lambda (w) "")
 #:draw
 (lambda (widget raw)
   (let* ((title (let ((t (assq-str widget 'title)))
                   (if (string-null? t) (assq-str widget 'id) t)))
          (lo (assq-num widget 'min 0))
          (hi (assq-num widget 'max 100))
          (hi (if (<= hi lo) (+ lo 1) hi))
          (step (let ((s (assq-num widget 'step 0.1)))
                  (if (<= s 0) 0.1 s)))
          (val (parse-val-from-raw raw)))
     (format #f
             "{\"kind\":\"slider\",\"id\":~s,\"title\":~s,\"value\":~a,\"min\":~a,\"max\":~a,\"step\":~a}"
             (assq-str widget 'id)
             title
             val lo hi step))))

(widget-type-register!
 #:type 'sensor
 #:label "Sensor"
 #:tags '(digital analog live input output)
 #:fields
 '((title (type string) (label "Titolo") (default ""))
   (refresh-sec (type int) (label "Refresh (s)") (default "5")))
 #:prepare (lambda (w) "")
 #:draw
 (lambda (widget raw)
   (let* ((title (let ((t (assq-str widget 'title)))
                   (if (string-null? t) (assq-str widget 'id) t)))
          (val (parse-val-from-raw raw)))
     (format #f
             "{\"kind\":\"sensor\",\"id\":~s,\"title\":~s,\"value\":~a}"
             (assq-str widget 'id)
             title
             val))))

(define (window->interval w)
  (let ((win (assq-str w 'window)))
    (cond ((string=? win "1h") "INTERVAL 1 HOUR")
          ((string=? win "6h") "INTERVAL 6 HOUR")
          ((string=? win "7d") "INTERVAL 7 DAY")
          (else "INTERVAL 24 HOUR"))))

(define (resolution->seconds w)
  (let ((r (assq-str w 'resolution)))
    (cond ((string=? r "1m") 60)
          ((string=? r "15m") 900)
          ((string=? r "1h") 3600)
          (else 300))))

(widget-type-register!
 #:type 'chart
 #:label "Grafico"
 #:tags '(analog history)
 #:fields
 '((title (type string) (label "Titolo") (default ""))
   (window (type enum) (label "Finestra") (default "24h"))
   (resolution (type enum) (label "Risoluzione") (default "5m"))
   (refresh-sec (type int) (label "Refresh (s)") (default "30")))
 #:prepare
 (lambda (widget)
   (let* ((label (assq-str widget 'entity-label))
          (lab (if (string-null? label) (assq-str widget 'entity-name) label))
          (bucket (resolution->seconds widget))
          (iv (window->interval widget)))
     (format #f
"SELECT FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(h.timestamp)/~a)*~a) AS t, AVG(h.value) AS v
 FROM history h
 JOIN labels l ON l.id = h.label_id
 WHERE l.name = '~a'
   AND h.timestamp >= NOW() - ~a
 GROUP BY 1
 ORDER BY 1 ASC"
             bucket bucket lab iv)))
 #:draw
 (lambda (widget raw)
   (let* ((title (let ((t (assq-str widget 'title)))
                   (if (string-null? t) (assq-str widget 'id) t))))
     ;; Pass through SQL JSON rows as series; shell plots t/v
     (format #f
             "{\"kind\":\"chart\",\"id\":~s,\"title\":~s,\"rows\":~a}"
             (assq-str widget 'id)
             title
             (if (and (string? raw) (> (string-length raw) 0)) raw "[]")))))
