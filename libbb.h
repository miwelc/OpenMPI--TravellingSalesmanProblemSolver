/* ************************************************************************ */
/*    Libreria para Branch-Bound, manejo de pila y Gestion de memoria       */
/* ************************************************************************ */

#include <cstdio>
const unsigned int MAXPILA = 150;

#ifndef NULO_T
#define NULO_T
const int NULO = -1;
#endif

#ifndef INFINITO_T
#define INFINITO_T
const long int INFINITO = 100000;
#endif

extern unsigned int NCIUDADES;



#ifndef TARCO_T
#define TARCO_T
struct tArco {
	int   v;
	int   w;
};
#endif


#ifndef TNODO_T
#define TNODO_T
class tNodo {
	
public:
	int* datos;

		
		tNodo() {
			datos = new int[2 * NCIUDADES];
		}
		
		int ci(){
			return datos[0];
		}

		int orig_excl(){
			return datos[1];
		}

		int* incl(){
			return &datos[2];
		}

		int* dest_excl(){
			return &datos[NCIUDADES + 2];
		}
		~tNodo() {
			delete [] datos;
		}
	
		
};
#endif


#ifndef PILA_T
#define PILA_T

class tPila{
	public:
		int tope;
		int* nodos;

		
			
		tPila(){
			tope = 0;
			nodos = new int[2 * NCIUDADES * MAXPILA];
		}

		inline bool llena () const{
			return (tope == 2 * NCIUDADES * MAXPILA);
		}

		inline bool vacia () const{
			return (tope == 0);
		}

		inline int tamanio () const{
			return (tope / (2 * NCIUDADES));
		}

		bool push (tNodo & nodo);

		bool pop (tNodo & nodo);

		bool divide (tPila& pila2);

		void acotar (int U);

		~tPila(){
			delete [] nodos;
		}
};
#endif



/* ********************************************************************* */
/* *** Cabeceras de funciones para el algoritmo de Branch-and-Bound *** */
/* ********************************************************************* */

void LeerMatriz (char archivo[], int** tsp) ;

bool Inconsistente  (int** tsp);
  /* tsp   -  matriz de inicidencia del problema o subproblema            */
  /* Si un subproblema tiene en alguna fila o columna todas sus entradas  */
  /* a infinito entonces es inconsistente: no conduce a ninguna solucion  */
  /* del TSP                                                              */

void Reduce (int** tsp, int *ci);
  /* tsp   -  matriz de incidencia del problema o subproblema        */
  /* ci    -  cota Inferior del problema                             */
  /* Reduce la matriz tsp e incrementa ci con las cantidades         */
  /* restadas a las filas/columnas de la matriz tsp.                 */
  /* La matriz reducida del TSP se obtiene restando a cada           */
  /* entrada de cada fila/columna de la matriz de incidencia,        */
  /* la entrada minima de dicha fila/columna. La cota inferior del   */
  /* (sub)problema es la suma de las cantidades restadas en todas    */
  /* las filas/columnas.                                             */

bool EligeArco (tNodo *nodo, int** tsp, tArco *arco);
  /*  nodo  -  descripcion de trabajo de un nodo del arbol (subproblema)   */
  /*  tsp   -  matriz de incidencia del subproblema (se supone reducida)   */
  /*  Busca una arco con valor 0 en una fila de la que no se haya incluido */
  /*  todavia ningun arco.                                                 */

void IncluyeArco(tNodo *nodo, tArco arco);
  /* nodo    - descripcion abreviada de trabajo                  */
  /* arco    - arco a incluir en la descripcion de trabajo       */
  /* Incluye el arco 'arco' en la descripcion de trabajo 'nodo'  */

bool ExcluyeArco(tNodo *nodo, tArco arco);
  /* nodo  - descripcion abreviada de trabajo                           */
  /* arco  - arco a excluir (explicitam) en la descripcion de trabajo   */
  /* Excluye el arco en la descripcion de trabajo 'nodo'                */

void PonArco(int** tsp, tArco arco);
  /*  tsp   -   matriz de incidencia                             */
  /*  Pone las entradas <v,?> y <?,w> a infinito, excepto <v,w>  */

void QuitaArco(int** tsp, tArco arco);
  /* tsp   -    matriz de incidencia                          */
  /* Pone la entrada <v,w> a infinito (excluye este arco)     */

void EliminaCiclos(tNodo *nodo, int** tsp);
  /* Elimina en 'tsp' los posibles ciclos que puedan formar los arcos */
  /* incluidos en 'nodo'                                              */

void ApuntaArcos(tNodo *nodo, int** tsp);
/* Dada una descripcion de trabajo 'nodo' y una matriz de inicidencia 'tsp' */
/* llama a Pon.Arco() para los arcos incluidos en 'nodo' y a Quita.Arco()   */
/* para los excluidos. Despues llama a Elimina.Ciclos para eliminar ciclos  */
/* en los caminos                                                           */

void InfiereArcos(tNodo *nodo, int** tsp);
  /* Infiere nuevos arcos a incluir en 'nodo' para aquellas filas que  */
  /* tengan N.ciudades-2 arcos a infinito en 'tsp'                     */

void Reconstruye (tNodo *nodo, int** tsp0, int** tsp);
  /* A partir de la descripcion del problema inicial 'tsp0' y de la  */
  /* descripcion abreviada de trabajo 'nodo', construye la matriz de */
  /* incidencia reducida 'tsp' y la cota Inferior 'ci'.              */

void HijoIzq (tNodo *nodo, tNodo *lnodo, int** tsp, tArco arco);
  /* Dada la descripcion de trabajo 'nodo', la matriz de incidencia        */
  /* reducida 'tsp', construye la descripcion de trabajo 'l.nodo'          */
  /* a partir de la inclusion del arco                                     */

void HijoDch (tNodo *nodo, tNodo *rnodo, int** tsp, tArco arco);
  /* Dada la descripcion de trabajo 'nodo' y la matriz de incidencia */
  /* reducida 'tsp', construye la descripcion de trabajo 'r.nodo'    */
  /* a partir de la exclusion del arco <v,w>.                        */

void Ramifica (tNodo *nodo, tNodo *rnodo, tNodo *lnodo, int** tsp0);
  /* Expande nodo, obteniedo el hijo izquierdo lnodo    */
  /* y el hijo derecho rnodo                            */

bool Solucion(tNodo *nodo);
  /* Devuelve TRUE si la descripcion de trabajo 'nodo' corresponde a una */
  /* solucion del problema (si tiene N.ciudades arcos incluidos).        */

int Tamanio (tNodo *nodo);
  /* Devuelve el tamanio del subproblema de la descripcion de trabajo nodo */
  /* (numero de arcos que quedan por incluir)                              */

void InicNodo (tNodo *nodo);
  /* Inicializa la descripcion de trabajo 'nodo' */

void CopiaNodo (tNodo *origen, tNodo *destino);
  /* Copia una descripcion de trabajo origen en otra destino */

void EscribeNodo (tNodo *nodo);
  /* Escribe en pantalla el contenido de la descripcion de trabajo nodo */


/* ******************************************************************** */
// Funciones para reserva dinamica de memoria
int ** reservarMatrizCuadrada(unsigned int orden);
void liberarMatriz(int** m);
/* ******************************************************************** */

