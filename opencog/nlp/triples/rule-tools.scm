;
; rules.scm
; Copyright (c) 2009 Linas Vepstas
;
; A set of routines for constructing ImplicationLinks and 
; VariableScopeLinks in a "simplified" manner. The goal here is
; to make it easier to write, read and debug hand-written 
; Implication/VarScope links.
;
; All of the names of the routines here begin with the string "r-".
; Most of the routines take as input, or create as output, an object
; called an "r-expression".  The "r-expression" is a very simple object:
; it is just a scheme association-list with two items: "clauses" and
; "vardecls".  The "clauses" are just a list of clauses that will
; eventually be anded together to create an ImplicationLink. The 
; "vardecls" is a list of variable declarations that will appear in the
; final VariableScopeLink that is constructed.
;
; A "real-life" example is given below; it constructs the
; VariableScopeLink needed to convert the parsed sentence 
; "Lisbon is the capital of Portugaul" into the semantic triple
; "capital_of(Porutgaul, Lisbon)".
;
;   (define my-varscope-link
;
;      ; Sentence: "Lisbon is the capital of Portugaul"
;      ; The RelEx parse is:
;      ;   _subj(be, Lisbon)
;      ;   _obj(be, capital)
;      ;   of(capital, Portugaul)
;      ;
;      ; We expect the following variable grounding:
;      ; var0=Lisbon, var1=capital var2=Portugaul
;      ;
;      (r-varscope
;         (r-and
;            ; We are looking for sentences anchored to the 
;            ; triples-processing node.
;            (r-anchor-trips "$sent")
;         
;            ; $var0 and $var1 are word-instances that must 
;            ; belong to the same sentence.
;            (r-decl-word-inst "$var0" "$sent") 
;            (r-decl-word-inst "$var1" "$sent")
;         
;            ; Match subject and object as indicated above.
;            (r-rlx "_subj" "be" "$var0")
;            (r-rlx "_obj" "be" "$var1")
;      
;            ; Match the proposition
;            (r-rlx "$prep" "$var1" "$var2")
;   
;            ; Get the lemma form of the word instance
;            (r-decl-lemma "$var1" "$word1")
;   
;            ; Convert to a phrase
;            (r-rlx "$phrase" "$word1" "$prep")
;         )
;         ; The implicand
;         (r-rlx "$phrase" "$var2" "$var0")
;      )
;   )
;
; The comparable IF...THEN expression to the above is:
;
; # IF %ListLink("# APPLY TRIPLE RULES", $sent)
;       ^ %WordInstanceLink($var0,$sent)  ; $var0 and $var1 must be
;       ^ %WordInstanceLink($var1,$sent)  ; in the same sentence
;       ^ _subj(be,$var0)
;       ^ _obj(be,$var1)
;       ^ $prep($var1,$var2)              ; preposition 
;       ^ %LemmaLink($var1,$word1)        ; word of word instance
;       ^ $phrase($word1, $prep)          ; convert to phrase
;       THEN ^3_$phrase($var2, $var0) 
;
; Some quick notes on evaluation: conjuncts are evaluated from 
; first to last, and so the order of the terms matters. Terms that
; narrow down the search the most dramatically should come first,
; so as to avoid an overly-broad search of the atomspace.
;
; -----------------------------------------------------------------
; get-anon-var-id! given a string, return a new unique string
; Given an input string, this returns a new, more-or-less unique
; new string.  Only minimal measures are taken to ensure uniqueness;
; so the result is not "strong". This is intended for use in generating
; "unique" variable names in ImplicatinLinks
;
; Example usage:
;     > (get-anon-var-id! "beee")
;     > $anon-beee-3

(define *anon-var-id* 0)
(define *anon-prefix* "$anon-")
(define (get-anon-var-id! str)
	(set! *anon-var-id* (1+ *anon-var-id*))
	(string-concatenate/shared
		(list *anon-prefix* str "-"
			(call-with-output-string 
				(lambda (port)
					(display *anon-var-id* port)
				)
			)
		)
	)
)

(define (set-anon-prefix! str) (set! *anon-prefix* str))

; -----------------------------------------------------------------
; r-isvar? -- return #t if arg is a variable name, i.e. starts with $
; Returns #t if the string starts with a $

(define (r-isvar? v)
	(eq? #\$ (string-ref v 0))
)

; -----------------------------------------------------------------
; r-rlx -- format a RelEx-like expression
;
; The r-rlx routine takes a RelEx-like expression and outputs a list
; of appropriate OpenCog linbk structures.  Thus, for example
; _subj(be, $var) is exprssed as (r-rlx "_subj" "be" "$var")
; which is then processed into the appropriate OpenCog graph.
; An appropriate indirection for "be" as a word-instance of the 
; word "be" is made.
;
; Example:
;      (r-rlx "_subj" "be" "$var0")   ;; _subj(be, $var0)
;
; This routine returns a r-expression containing the clause and the
; variables appearing in that clause.
;
(define (r-rlx rel a b)

	; The lemma-link needed for joining words to wod-instances.
	(define (lem var wrd)
		(LemmaLink (stv 1 1)
			(VariableNode var)
			(WordNode wrd)
		)
	)

	; Create a basic variable typing declaration
	(define (var-type var type)
		(TypedVariableLink
			(VariableNode var)
			(VariableTypeNode type)
		)
	)

	; prepend to list, maybe
	(define (vpend bool name lst)
		(define (pend bool item lst)
			(if bool (cons item lst) lst)
		)
		(pend bool (var-type name "WordInstanceNode") lst)
	)

	(let* (
			; av, bv are true if a,b start with $
			(av (r-isvar? a))
			(bv (r-isvar? b))
			(rv (r-isvar? rel))

			; avn is the variable name to use
			(avn (if av a (get-anon-var-id! a)))
			(bvn (if bv b (get-anon-var-id! b)))
			(rvn (if rv rel (get-anon-var-id! rel)))

			; The basic RelEx predicate structure
			(pred
				(EvaluationLink (stv 1 1)
					(if rv
						(VariableNode rvn)
						(DefinedLinguisticRelationshipNode rel)
					)
					(ListLink
						(VariableNode avn)
						(VariableNode bvn)
					)
				)
			)
		)

		; The clauses needed to work with this relex expression
		(define clauses
			(cond
				((and av bv) (list pred))
				((and (not av) bv)
					(list pred (lem avn a))
				)
				((and av (not bv))
					(list pred (lem bvn b))
				)
				((and (not av) (not bv))
					(list pred (lem avn a) (lem bvn b))
				)
			)
		)
		
		; The variables appearing in the clauses
		(define vartypes
			(vpend (not av) avn
				(vpend (not bv) bvn '())
			)
		)

		; Return the variables and clauses in an association list
		(alist-cons 'vardecls vartypes
			(alist-cons 'clauses clauses '())
		)
	)
)

; -----------------------------------------------------------------
; Utility routines to fetch the components of the r-expression

(define (r-get-vardecls expr)
	(let ((evp (assoc 'vardecls expr)))
		(if evp (cdr evp) '())
	)
)

(define (r-get-clauses expr)
	(let ((evp (assoc 'clauses expr)))
		(if evp (cdr evp) '())
	)
)

; -----------------------------------------------------------------
; r-not -- invert the truth-value of an r-expression
; Wrap a given r-expression with a NotLink
;
; Example usage:
;    (r-not (r-rlx "_subj" "be" "$var0"))
;
(define (r-not expr)

	; a(def
	; ;xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	; NOT DONE

	; split out the 
	(let* (
			; ev == expr variables
			(ev (r-get-vardecls expr))
			; ec == expr clauses
			(ec (r-get-clauses expr))
		)
	)
)

; -----------------------------------------------------------------
; r-and -- concatenate a list of r-expressions together.
;
; Accepts a variable number of r-expressions and returns a concatenation 
; of them into one r-expression structure. The order of the clauses is
; preserved.
;
; Example usage:
;    (r-and
;       (r-rlx "_subj" "be" "$var0")
;       (r-rlx "_obj" "be" "$var1")
;    )
;
(define (r-and . alst)

	; Merge two alists together into one.
	(define (merge item otem)
		(let* (
				; iv == item variables
				(iv (r-get-vardecls item))
				
				; ic == item clauses
				(ic (r-get-clauses item))
				
				; ov == other item variables
				(ov (r-get-vardecls otem))
				
				; oc == other item clauses
				(oc (r-get-clauses otem))

				;; concatenate
				(varbles (append ov iv))
				(clauses (append oc ic))
			)

			; Return the variables and clauses in an association list
			(alist-cons 'vardecls varbles
				(alist-cons 'clauses clauses '())
			)
		)
	)

	; concatenate a bunch of alists into one
	(define (do-merge lst reslt)
		(if (not (null? lst))
			(do-merge (cdr lst)
				(merge (car lst) reslt)
			)
			reslt
		)
	)

	; And now do it.
	(do-merge alst '())
)

; -----------------------------------------------------------------
; r-link -- declare a simple opencog link connecting two items
;
; Returns an r-expression defining the link.
; 
; Example usage:
;   (r-link WordInstanceLink "$var1" "$sent")
;
(define (r-link lnk-type item-1 item-2)
	(define lnk
		(lnk-type (stv 1 1)
			(VariableNode item-1)
			(VariableNode item-2)
		)
	)
	(alist-cons 'clauses (list lnk) '())
)

; -----------------------------------------------------------------
; r-decl-var -- declare a variable type
;
; During pattern matching, a variable can be constrained to be of a 
; a certain type, so that it is not matched too freely. This is done
; by using TypedVariableLink's and VariableTypeNode's. 
;
; This routine returns an r-expression.
;
; Example usage:
;	(r-decl-var "WordInstanceNode" "$var0")
;
(define (r-decl-var vartype varname)
	(define (vd vtype vname)
		(TypedVariableLink
			(VariableNode vname)
			(VariableTypeNode vtype)
		)
	)
	(alist-cons 'vardecls (list (vd vartype varname)) '())
)

; -----------------------------------------------------------------
; r-varscope -- create a varscope structure from r-expressions
;
; A VariableScopeLink consists of an implication link P->Q, with the
; predicate P being a sequence of disjuncts (to be conjoined together)
; and Q the implicand.  Here, both P and Q are taken to be r-expressions,
; constructed with the r-* routines.  A VariableScopeLink also contains
; a list of the bound variables which will be grounded when the 
; implication is evaluated. The variables are taken from the predicate 
; P r-expression. Any variables appearing in Q must also appear in P.
;
; Returns the constructed VariableScopeLink.
;
(define (r-varscope predicates implicand)
	(let* (
			; pv == predicates variables
			(pv (r-get-vardecls predicates))
				
			; pc == predicates clauses
			(pc (r-get-clauses predicates))

			; ic == implicand clauses
			(ic (r-get-clauses implicand))
		)

		; The Big Kahuna -- a list of variables, and the implication.
		(VariableScopeLink
			(ListLink pv)
			(ImplicationLink 
				(AndLink pc)
				ic
			)
		)
	)
)

; -----------------------------------------------------------------
; r-schema -- declare a GroundedSchemaNode
;
; Returns an r-expression defining the schema
; 
; Example usage:
;   (r-schema "scm:make-prep-phrase" "$word1" "$prep")
;
(define (r-schema schema-name item-1 item-2)
	(define lnk
		(ExecutionLink
			(GroundedSchemaNode schema-name)
			(ListLink
				(VariableNode item-1)
				(VariableNode item-2)
			)
		)
	)
	(alist-cons 'clauses (list lnk) '())
)

; -----------------------------------------------------------------
; -----------------------------------------------------------------
; -----------------------------------------------------------------
; The routines below are "utilities" aimed primarily with the 
; specifics of the triples-processing code.  These are not "core"
; routines like those above.
;
; -----------------------------------------------------------------
; r-anchor -- link a variable to an AnchorNode
;
; Returns an r-expression.
;
(define (r-anchor anchor-name var)
	(define lnk
		(ListLink (stv 1 1)
			(AnchorNode anchor-name)
			(VariableNode var)
		)
	)
	(alist-cons 'clauses (list lnk) '())
)

; -----------------------------------------------------------------
; r-anchor-trips -- declare a sentence anchored to the triples anchor
;
; Returns an r-expression
;
(define (r-anchor-trips sent)
	(r-and 
		(r-anchor "# APPLY TRIPLE RULES" sent)
		(r-decl-var "ParseNode" sent)
	)
)

; -----------------------------------------------------------------
; r-decl-word-inst -- declare a word instance belonging to a sentence
;
; This routine presumes that "word-inst" is a variable, and declares
; it as such. (It does NOT make sense to declare a non-variable to be 
; a part of a sentence -- this was already done during parse and
; shouldn't be redone).
;
; Returns an r-expression.
;
(define (r-decl-word-inst word-inst sent)
	(r-and
		(r-link WordInstanceLink word-inst sent)
		(r-decl-var "WordInstanceNode" word-inst)
	)
)

; -----------------------------------------------------------------
; r-decl-lemma -- declare a lemma of a word-instance
;
; If "lemma" is a variable, then it is declared to be typed as
; a WordNode
;
; Returns an r-expression.
;
(define (r-decl-lemma word-inst lemma)
	(define rl (r-link LemmaLink word-inst lemma))

	; If lemma is a string beginging with $, then declare it
	; to be a variable that must be a WordNode.
	(if (r-isvar? lemma)
		(r-and rl (r-decl-var "WordNode" lemma))
		rl
	)
)

; -----------------------------------------------------------------
; r-decl-prep -- declare a preposition relation
; In order to get a meaningful result here, the "preps.scm" 
; preposition dictionary must be loaded.
;
; Assumes that both "prep" and "prep-word" are vairables, 
; and declares "prep" to be of type DefinedLinguisticRelationshipNode
;
; Returns an r-expression.
;
(define (r-decl-prep prep prep-word)

	(r-and 
		(r-link ListLink prep prep-word)
		(r-decl-var "DefinedLinguisticRelationshipNode" prep)
	)
)

; -----------------------------------------------------------------
; r-rlx-flag -- create a ReleEx flag-style relationship
;
; Returns an r-expression defining the flag.
; 
; Example usage:
;   (r-rlx-flag "definite" "$var2")
;
; creates the equivalent to the RelEx DEFINITE-FLAG($var2)
;
(define (r-rlx-flag flag var)
	(define lnk
		(InheritanceLink (stv 1 1)
			(VariableNode var)
			(DefinedLinguisticConceptNode flag)
		)
	)
	(alist-cons 'clauses (list lnk) '())
)

; ------------------------ END OF FILE ----------------------------
; -----------------------------------------------------------------
