/*
 * =====================================================================================
 *
 *       File Nom:  tp1.c
 *
 *    Description:  TP1 effectué dans le cadre du cours IFT2035, 
 *                  Concepts des langages de programmation.
 *                  ÉTÉ 2014
 *
 *        Version:  1.0
 *        Created:  06/05/2014 23:44:47
 *       Revision:  none
 *       Compiler:  gcc
 *
 *        Authors:  Champagne, Pascal & Descoteaux, Felix 
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
#include "debug.h"
*/

/*-----------------------------------------------------------------------------
 *  DEFINITIONS
 *-----------------------------------------------------------------------------*/

#define tp1_debug 1
//#define stack_debug
#define TRUE 1
#define FALSE 0
#define CHIFFRES "0123456789"
static char *yes_syntaxe = "ERREUR DE SYNTAXE DANS L’EXPRESSION: ";
static char *no_syntaxe = "Une erreur est survenue lors du traîtement de l'expression: ";

//Free pointer X
#define FREE_PTR(x) \
    if (x) { \
        free(x); \
        x = NULL; \
    }

/*-----------------------------------------------------------------------------
 *  TYPES DEFINITIONS
 *-----------------------------------------------------------------------------*/


typedef int bool;


// prototypes de fonction pour les opérations binaires.
// utilisation de void * pour le contexte pour éviter erreur de compilation de gcc.
bool addition(void *ctx, double gauche, double droite, double *resultat);
bool soustraction(void *ctx, double gauche, double droite, double *resultat);
bool multiplication(void *ctx, double gauche, double droite, double *resultat);
bool division(void *ctx, double gauche, double droite, double *resultat);

// Pointeur à une function pour les opérations binaires
typedef bool (*t_op_funct)(void *ctx, double gauche, double droite, double *resultat);

// Structure décrivant un opérateur
typedef struct _op {
    char symbole;
    char precedence;
    char dependance;
    t_op_funct funct;
} t_op; 

// Structure pour les chaînes de caractères.
typedef struct _str {
    long alloc;
    long util;
    char *str;
} t_str;

// Structure décrivant une expression
typedef struct _expr {
  //Type is either 'operation' or 'number', hence the union
  enum {
    OP = 1, 
    NOMBRE
  } type;
  union {
    struct {
      char *ptr;
      int  len;
    } nombre;
    struct {
      t_op *op;
      struct _expr *gauche;
      struct _expr *droite;
    } op;
  } _;
} t_expr;

// Structure pile de termes
typedef struct {
    int alloc; // nombre d'éléments alloués
    int util; // nombre d'éléments dans la pile
    t_expr **termes;
} t_pile_termes;

typedef enum {
    NO_ERROR_YET,
    MISSING_RIGHT_TERM,
    MISSING_LEFT_TERM,
    UNKNOWN_CHARACTER,
    TOO_MANY_TERMS,
    MEMORY_ERROR,
    DIVIDE_BY_ZERO,
    CONVERSION_TO_DOUBLE_ERROR1,
    CONVERSION_TO_DOUBLE_ERROR2,
    CONVERSION_TO_DOUBLE_ERROR3,
    ADDITION_CARRY,
    ADDITION_NEG_PLUS_NEG_GIVING_POS,
    ADDITION_POS_PLUS_POS_GIVING_NEG,
    NOTHING_TO_PROCESS
} t_error_id;

// structure qui est utilisée tout au long du traitement de l'expression en entrée.
typedef struct {
  t_str         *expression;
  t_pile_termes pile_termes;
  t_expr        *ASA;
  struct {
    t_error_id  id;
    char        *msg;
    int         ligne;
    char        *syntaxe;
  } erreur; 
  double        resultat;
} t_contexte_execution;

/*-----------------------------------------------------------------------------
 *  INITIALIZATIONS
 *-----------------------------------------------------------------------------*/


//List of possible operations
//symbol, precedance, dependance, op type
static t_op liste_op[] = { 
    {'+', 0x00, FALSE, addition}, 
    {'-', 0x00, TRUE, soustraction}, 
    {'*', 0x01, FALSE, multiplication}, 
    {'/', 0x01, TRUE, division} 
};

//List of postscript operations
static char *postscript_op[] = {"add", "sub", "mul", "div"};


/*-----------------------------------------------------------------------------
 *  ERROR DEBUGGING LEVEL
 *-----------------------------------------------------------------------------*/


//Pre processing for error debugging
#ifdef stack_debug
#define DEBUG_STACK(s) \
    print_stack(s);
#else
#define DEBUG_STACK(s)
#endif

#define DEBUG_STRING(x) \
  printf("%s->alloc:%ld\n", #x, x->alloc); \
  printf("%s->util :%ld\n", #x, x->util); \
  printf("%s->str  :%s\n", #x, x->str ? x->str : "NULL")

#define DEBUG_EXPR(x) \
  if (x->type == OP) { \
    printf("  type: OP\n"); \
    printf("    op: %c\n", x->_.op.op->symbole); \
    printf("gauche: %p\n", x->_.op.gauche); \
    printf("droite: %p\n", x->_.op.droite); \
  } else if (x->type == NOMBRE) { \
    printf("  type: NOMBRE\n"); \
    printf("nombre: %.*s\n", x->_.nombre.len, x->_.nombre.ptr); \
  } else { \
    printf("  type: %c\n", x->type); \
  }

/*-----------------------------------------------------------------------------
 *  FUNCTIONS DEFINITIONS
 *-----------------------------------------------------------------------------*/


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void affecter_erreur(t_error_id id, t_contexte_execution *ctx, int ligne)
 *  Description:  Assigne une erreur au contexte d'exécution
 * =====================================================================================
 */
void affecter_erreur(t_error_id id, t_contexte_execution *ctx, int ligne) {

    ctx->erreur.id = id;
    ctx->erreur.ligne = ligne;
    ctx->erreur.syntaxe = no_syntaxe;

    switch(id) {

        case MISSING_RIGHT_TERM:
        case MISSING_LEFT_TERM:
            ctx->erreur.msg = "Terme de manquant.  Pas assez d'opérandes disponible pour le traîtement d'un opérateur.\n";
            ctx->erreur.syntaxe = yes_syntaxe;
            break;

        case UNKNOWN_CHARACTER:
            ctx->erreur.msg = "Caractère inconnu\n";
            break;

        case TOO_MANY_TERMS:
            ctx->erreur.msg = "Trop de termes pour le nombre d'opérateur\n";
            ctx->erreur.syntaxe = yes_syntaxe;
            break;

        case MEMORY_ERROR:
            ctx->erreur.msg = "Problème lors de l'allocation de memoire ";
            break;

        case DIVIDE_BY_ZERO:
            ctx->erreur.msg = "Divison par zero\n";
            break;

        case CONVERSION_TO_DOUBLE_ERROR1:
            ctx->erreur.msg = "Erreur de conversion à un double.  L'expression ne peut pas être évaluée de façon précise.\n";
            break;            

        case CONVERSION_TO_DOUBLE_ERROR2:
            ctx->erreur.msg = "Erreur de conversion à un double.  Aucun caractère traité.\n";
            break;            

        case CONVERSION_TO_DOUBLE_ERROR3:
            ctx->erreur.msg = "Erreur de conversion à un double.  Longueur de chaîne traitée différente de prévue par programmation.\n";
            break;            

        case ADDITION_CARRY:
            ctx->erreur.msg = "Erreur d'addition.  Le résultat est plus petit ou plus grand que les opérandes, selon le signe de celle-ci.\n";
            break;            

        case ADDITION_NEG_PLUS_NEG_GIVING_POS:
            ctx->erreur.msg = "Erreur d'addition.  nombre négatif + nombre négatif = nombre positif.\n";
            break;            

        case ADDITION_POS_PLUS_POS_GIVING_NEG:
            ctx->erreur.msg = "Erreur d'addition.  nombre positif + nombre positif = nombre négatif.\n";
            break;            

        case NOTHING_TO_PROCESS:
            ctx->erreur.msg = "Expression à blanc.\n";
            break;            
            
        default:
            ctx->erreur.msg = "Code d'erreur inconnu\n";
            break;
    }
}

#ifdef stack_debug
/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void print_t_expr(t_expr* ex)
 *  Description:  Print t_expr
 * =====================================================================================
 */
void print_atomic_t_expr(t_expr* ex)
{
    if (ex->type==NOMBRE)
        printf(" %.*s ", ex->_.nombre.len, ex->_.nombre.ptr);
    else if (ex->type==OP)
        printf(" %c ", ex->_.op.op->symbole);
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void print_stack(t_pile_termes* p)
 *  Description:  Prints stack p content.
 * =====================================================================================
 */
void print_stack(t_pile_termes* p)
{
    int i;

    printf("Stack: ");

    if (p->util == 0) printf(" empty "); 
    for(i = 0; i < p->util; i++) { 
        print_atomic_t_expr(p->termes[i]);
    }
    printf("\n");
}
#endif


/*-----------------------------------------------------------------------------
 *  FONCTIONS associé aux opérations permises dans la vocabulaire du language 
 *            Ces fonctions seront appelés durant le traitement calculant le
 *            résultat de l'expression.  Elles seront appelées au moyen du pointeur
 *            à une fonction défini dans la structure t_op à l'attribut funct.
 *-----------------------------------------------------------------------------
 */
/*
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool addition(void *ctx, double gauche, double droite, double *resultat)
 *  Description:  Somme de gauche et droite
 * =====================================================================================
 */
bool addition(void *ctx, double gauche, double droite, double *resultat)
{
  *resultat = gauche + droite;
  return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool soustraction(void *ctx, double gauche, double droite, double *resultat)
 *  Description:  Différence de gauche et droite
 * =====================================================================================
 */
bool soustraction(void *ctx, double gauche, double droite, double *resultat)
{
    *resultat = gauche - droite;
    return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool multiplication(void *ctx, double gauche, double droite, double *resultat)
 *  Description:  Produit de gauche et droite
 * =====================================================================================
 */
bool multiplication(void *ctx, double gauche, double droite, double *resultat)
{
    *resultat = gauche * droite;
    return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool division(void *ctx, double gauche, double droite, double *resultat)
 *  Description:  Quotient de gauche et droite
 * =====================================================================================
 */
bool division(void *ctx, double gauche, double droite, double *resultat)
{
    if (!droite) { 
        affecter_erreur(DIVIDE_BY_ZERO, ctx, 0);
        return FALSE;
    }

    *resultat = gauche / droite;
    return TRUE;
}



/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  t_op *type_op(char c)
 *  Description:  Return the operator type of a character
 * =====================================================================================
 */
t_op *type_op(char c)
{
  long i, end = sizeof(liste_op) / sizeof(liste_op[0]);

  for(i = 0; i < end; i++)
    if (liste_op[i].symbole == c)
      return &liste_op[i];

  return NULL;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool alloc_str(t_contexte_execution *ctx, long taille, t_str **str, 
 *                                 int ligne, bool etendre)
 *  Description:  Crée une structure t_str et initialise les données tel qu'énoncé par les 
 *                paramètres.  Permet la reallocation lorsque etendre != FALSE
 * =====================================================================================
 */
static bool alloc_str(t_contexte_execution *ctx, long taille, t_str **str, int ligne, bool etendre)
{
  t_str *temp;

  temp = etendre ? (t_str *)realloc(*str, sizeof(t_str) + sizeof(char) * taille)
                 : (t_str *)malloc(sizeof(t_str) + sizeof(char) * taille);
  if (!temp) {
    affecter_erreur(MEMORY_ERROR, ctx, ligne);
    return FALSE;
  }
  temp->alloc = taille;
  temp->str = (char *)(temp + 1);  // premier byte au delà de la struct t_str allouée plus haut

  *str = temp;

  return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void freeString(t_str **str)
 *  Description:  libère la string pointée par *str
 * =====================================================================================
 */
static void liberer_str(t_str **str)
{
  if (!str)
    return;

  FREE_PTR(*str);
}



/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool lire_expr_postfix(t_str *s, FILE *infile)
 *  Description:  Read expression from string  
 * =====================================================================================
 */
bool lire_expr_postfix(t_contexte_execution *ctx, FILE *stream)
{
    char c;
    ctx->expression->util = 0;
    
    do {

        c = fgetc(stream);
        if (feof(stream)) 
            break; 

        if (ctx->expression->util == ctx->expression->alloc) {
            if (!alloc_str(ctx, ctx->expression->alloc << 1, &ctx->expression, __LINE__, TRUE))
              return FALSE;
        }

        if (c == '\n') c = 0;

        ctx->expression->str[ctx->expression->util++] = c;

    } while(c);

    return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool push_pile_termes(t_pile_termes *p, t_expr *noeud)
 *  Description:  Push an expression on the stack, after increasing its size by 16
 * =====================================================================================
 */
bool push_pile_termes(t_contexte_execution *ctx, t_expr *noeud)
{
  t_pile_termes *p = &ctx->pile_termes;

  if (p->util == p->alloc) {
    if (!p->alloc) {
      p->alloc = 16;
      p->termes = (t_expr **)malloc(sizeof(t_expr *) * p->alloc);
    } else {
      p->alloc <<= 1;
      p->termes = (t_expr **)realloc(p->termes, sizeof(t_expr *) * p->alloc);
    }

    if (!p->termes) {
      affecter_erreur(MEMORY_ERROR, ctx, __LINE__);
      return FALSE;
    }
  }

  p->termes[p->util] = noeud;
  p->util++;
  DEBUG_STACK(p);

  return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool pop_pile_termes(t_pile_termes *p, t_expr **noeud)
 *  Description:  Pop an expression from the stack
 * =====================================================================================
 */
bool pop_pile_termes(t_contexte_execution *ctx, t_expr **noeud)
{
    t_pile_termes *p = &ctx->pile_termes;

    if (!p->util) //si la pile est vide
        return FALSE;

    p->util--; //decrementer nombre d'éléments de la pile
    *noeud = p->termes[p->util]; //on s'assure de retourner l'élément
    p->termes[p->util] = NULL; // puis on affecte NULL à l'élément qui pointe sur l'élément retourné

    DEBUG_STACK(p);

    return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void liberer_expr(t_expr **p)
 *  Description:  Free recursively memory of p
 * =====================================================================================
 */
void liberer_expr(t_expr **p)
{
  //Le pointeur est null
  if (*p == NULL)
    return;

  //Le pointeur est une t_expr de type OP
  if ((*p)->type == OP) {
    //Appels recursifs pour liberer les termes gauche et droit
    liberer_expr(&(*p)->_.op.gauche);
    liberer_expr(&(*p)->_.op.droite);
    FREE_PTR(*p);
    return;
  }

  if ((*p)->type == NOMBRE) {
    FREE_PTR(*p);
    return;
  }

  printf("Type de noeud inconnu!\n");
  DEBUG_EXPR((*p));

  exit(2);
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void liberer_termes_de_la_pile(t_pile_termes *p)
 *  Description:  Free stack 
 * =====================================================================================
 */
void liberer_termes_de_la_pile(t_contexte_execution *ctx)
{
  t_expr *temp;

  if (ctx->pile_termes.util) {
    while(pop_pile_termes(ctx, &temp))
      liberer_expr(&temp);
  }
}

/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  bool convertir_postfix_en_ASA(t_contexte_execution *ctx)
 *  Description:  Convert postfix expression to abstract syntax tree using a stack
 * =====================================================================================
 */
bool convertir_postfix_en_ASA(t_contexte_execution *ctx)
{
    char *p; 
    t_expr *temp;
    t_op *pOP;
    int len;
    bool succes = FALSE;

    if (!*ctx->expression->str) {
      affecter_erreur(NOTHING_TO_PROCESS, ctx, 0);
      return FALSE;
    }

    // boucle qui parcourt l'expression en entrée et bâtit l'ASA.
    for(p = ctx->expression->str, len = 1, temp = NULL; 
        *p; 
        p += len, len = 1, temp = NULL)
    {    
        if (isspace(*p)) continue;

        if ((pOP = type_op(*p))) {    // est-ce un opérateur

            temp = (t_expr *)malloc(sizeof(t_expr));
            if (!temp) {
              affecter_erreur(MEMORY_ERROR, ctx, __LINE__);
              break;
            }

            temp->type        = OP; 
            temp->_.op.op     = pOP;
            temp->_.op.droite = NULL;  // initialiser tous les champs pour assurer le bon fonctionnement
            temp->_.op.gauche = NULL;  // de liberer_expr() si un des appels à pop_pile_termes retournerait FALSE. 

            if (!pop_pile_termes(ctx, &temp->_.op.droite)) {
                affecter_erreur(MISSING_RIGHT_TERM, ctx, 0); 
                break;
            }

            if (!pop_pile_termes(ctx, &temp->_.op.gauche)) {
                affecter_erreur(MISSING_LEFT_TERM, ctx, 0); 
                break;
            }

            if (!push_pile_termes(ctx, temp)) 
                break;

        } else if ((len = strspn(p, CHIFFRES))) {  // est-ce un nombre

            temp = (t_expr *)malloc(sizeof(t_expr));
            if (!temp) {
              affecter_erreur(MEMORY_ERROR, ctx, __LINE__);
              break;
            }

            temp->type = NOMBRE;
            temp->_.nombre.ptr = p;
            temp->_.nombre.len = len;

            if (!push_pile_termes(ctx, temp))
                break;

        } else {  // catch all

            affecter_erreur(UNKNOWN_CHARACTER, ctx, 0); 
            break;
        }
    }  

    switch(ctx->pile_termes.util) {

      case 0:  
          if (ctx->erreur.id == NO_ERROR_YET)
            affecter_erreur(NOTHING_TO_PROCESS, ctx, 0); // cas où l'expression ne contient que des espaces et autres caractères considérés comme espace
          break; 
      case 1:
          pop_pile_termes(ctx, &ctx->ASA);
          succes = TRUE;
          break;
      default:
          if (ctx->erreur.id == NO_ERROR_YET) {
            if (ctx->pile_termes.util > 1) {
              affecter_erreur(TOO_MANY_TERMS, ctx, 0); 
            }
          }
          break;
    }

    if (!succes) {
        liberer_termes_de_la_pile(ctx);
        
        if (temp)
           liberer_expr(&temp);
    }

    return succes;
}



/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  void ajouter_parentheses(t_expr *parent, t_expr *gauche, bool *parenthese_gauche, t_expr *droite, bool *parenthese_droite)
 *  Description:  Détermine si l'ajout de parentheses à gauche et/ou à droite est nécessaire
 * =====================================================================================
 */
void ajouter_parentheses(t_expr *parent, 
                         t_expr *gauche, 
                         bool *parenthese_gauche, 
                         t_expr *droite, 
                         bool *parenthese_droite)
{
    *parenthese_gauche = FALSE;
    *parenthese_droite = FALSE;

    if (gauche->type == OP)
        *parenthese_gauche = parent->_.op.op->precedence > gauche->_.op.op->precedence;

    if (droite->type == OP) {
        *parenthese_droite = parent->_.op.op->precedence > droite->_.op.op->precedence;
        if (!*parenthese_droite) {
            *parenthese_droite = parent->_.op.op->precedence == droite->_.op.op->precedence && 
                                 parent->_.op.op->dependance;
        }
    }
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  printScheme(t_expr *p)
 *  Description:  Imprime l'expression en Scheme en parcourant l'ASA.
 * =====================================================================================
 */
void printScheme(t_expr *p) 
{
  if (p->type == NOMBRE) {
    printf(" %.*s", p->_.nombre.len, p->_.nombre.ptr);
    return;
  }

  printf(" (%c", p->_.op.op->symbole);
  printScheme(p->_.op.gauche);
  printScheme(p->_.op.droite);
  putchar(')');
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  printC(t_expr *p)
 *  Description:  Imprime l'expression en C en parcourant l'ASA.
 * =====================================================================================
 */
void printC(t_expr *p)
{
  bool parenthese_gauche, 
       parenthese_droite;

  if (p->type == NOMBRE) {
    printf("%.*s", p->_.nombre.len, p->_.nombre.ptr);
    return;
  }

  ajouter_parentheses(p, p->_.op.gauche, &parenthese_gauche, p->_.op.droite, &parenthese_droite);

  if (parenthese_gauche) putchar('(');
  printC(p->_.op.gauche);
  if (parenthese_gauche) putchar(')');

  printf("%c", p->_.op.op->symbole);

  if (parenthese_droite) putchar('(');
  printC(p->_.op.droite);
  if (parenthese_droite) putchar(')');
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  printPostscript(t_expr *p)
 *  Description:  Imprime l'expression en Postscript en parcourant l'ASA.
 * =====================================================================================
 */
void printPostscript(t_expr *p)
{
  if (p->type == NOMBRE) {
    printf(" %.*s", p->_.nombre.len, p->_.nombre.ptr);
    return;
  }

  printPostscript(p->_.op.gauche);
  printPostscript(p->_.op.droite);
  printf(" %s", postscript_op[p->_.op.op - liste_op]);
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  rapporter_expressions(t_str *s, t_expr *p, double resultat)
 *  Description:  Imprime les sorties sur stdout.
 * =====================================================================================
 */
void rapporter_expressions(t_contexte_execution *ctx)
{
  printf("    Scheme:");
  printScheme(ctx->ASA);
  putchar('\n');

  printf("         C: ");
  printC(ctx->ASA);
  putchar('\n');

  printf("Postscript:");
  printPostscript(ctx->ASA);
  putchar('\n');

  printf("    Valeur: %g\n\n", ctx->resultat);
}

/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  evaluer_expression(t_expr *p, double *result)
 *  Description:  Calcule le résultat de l'expression en entrée et store celui-ci dans 
 *                result.
 * =====================================================================================
 */
bool evaluer_expression(t_contexte_execution *ctx, t_expr *p, double *result)
{
  if (p->type == NOMBRE) {
    char *ptr;
    int len, 
        pow;
    /***
    Ici on convertit le nombre afin qu'on puisse l'utiliser les avec
    opérateurs + - * / du langage C.  Les données ont été validées lors
    de l'analyse de l'expression. 
    ***/
    *result = 0;
    ptr = &p->_.nombre.ptr[p->_.nombre.len - 1];
    pow = 1;
    for (len = p->_.nombre.len; len; len--) {
      *result += (*ptr - '0') * pow;
      ptr--;
      pow *= 10;
    }

    return TRUE;
  }

  double droite, gauche, res;

  if (!evaluer_expression(ctx, p->_.op.gauche, &gauche))
    return FALSE;

  if (!evaluer_expression(ctx, p->_.op.droite, &droite))
    return FALSE;

  if (!p->_.op.op->funct(ctx, gauche, droite, &res))
      return FALSE;

  *result = res;

  return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  init_contexte(t_contexte_execution *ctx) 
 *  Description:  Initialise les variables de contexte pour le traîtement d'une 
 *                expression.  À appeler avec chaque traîtement.
 * =====================================================================================
 */
void init_contexte(t_contexte_execution *ctx) 
{
  ctx->expression->util = 0;
  ctx->pile_termes.util = 0;
  liberer_expr(&ctx->ASA);
  ctx->erreur.id = NO_ERROR_YET;
  ctx->erreur.msg = NULL;
  ctx->erreur.ligne = 0;
  ctx->erreur.syntaxe = "?";
  ctx->resultat;
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  liberer_var_allouee_contexte(t_contexte_execution *ctx)
 *  Description:  Libère les variables alloué lors du contexte d'exécution du traîtement
 *                d'expressions.  
 * =====================================================================================
 */
void liberer_var_allouee_contexte(t_contexte_execution *ctx)
{
  liberer_termes_de_la_pile(ctx);
  if (ctx->pile_termes.alloc) {
    FREE_PTR(ctx->pile_termes.termes);
  }
  liberer_expr(&ctx->ASA);
  liberer_str(&ctx->expression);
}


/* 
 * ===  FUNCTION  ======================================================================
 *          Nom:  rapporter_erreur(t_contexte_execution *ctx) 
 *  Description:  Fonction qui imprime les erreurs rencontrées lors du traîtement
 *                d'une expressions.
 * =====================================================================================
 */
void rapporter_erreur(t_contexte_execution *ctx) 
{
  char str[128];

  if (ctx->erreur.ligne)
    sprintf(str, " à la ligne %d", ctx->erreur.ligne);
  else
    str[0] = 0;

  printf("%s%s --> %s%s", 
         ctx->erreur.syntaxe, 
         ctx->expression->str, 
         ctx->erreur.msg, 
         str);
}


/*-----------------------------------------------------------------------------
 *  MAIN
 *-----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  t_contexte_execution contexte;
  memset(&contexte, 0, sizeof(contexte));

  if (!alloc_str(&contexte, 0x400, &contexte.expression, __LINE__, FALSE))  // start with 1Kb
    return 1;

  for (;
       TRUE; 
       (contexte.erreur.id != NO_ERROR_YET) ? rapporter_erreur(&contexte) : TRUE) {

      init_contexte(&contexte);

      printf("EXPRESSION? ");

      if (!lire_expr_postfix(&contexte, stdin))
          continue; 

      if (feof(stdin)) {  // ctrl-d sets EOF
        printf("\n");
        break;
      }

      if (!convertir_postfix_en_ASA(&contexte))
          continue;

      if (!evaluer_expression(&contexte, contexte.ASA, &contexte.resultat))
          continue;

      rapporter_expressions(&contexte);
  }

  liberer_var_allouee_contexte(&contexte);

  return 0;
}
