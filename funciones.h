#define MINCEDER 3
#define TAGCARGA 0
#define TAGCOTA 1


enum ESTADO {
	PASIVO,
	ACTIVO,
	TERMINAR
};
enum TIPOMENSAJE {
	PETIC,
	NODOS, //trabajo
	TOKEN,
	FIN
};

enum COLOR {
	BLANCO,
	NEGRO,
	NO
};

struct MensajeCota {
	int origen;
	int cota;
};

struct Mensaje {
	int tipo;
	int numNodos;
	int colorToken;
	int origen;
};


int tenemosToken = NO;
int MICOLOR = BLANCO;

void Equilibrar_Carga(tPila *pila, int *estadoAct, int rank, int size) {
	Mensaje mensaje, mensajeTmp;
	tNodo nodoTmp;
	MPI_Status status;
	int hayMensajes;
	int *vdatos;
	
	if(pila->vacia()) {
		//Enviamos petición al proceso (rank+1)%size
		//printf("Soy %i: no tengo nodos, se los pido a %i\n", rank, (rank+1)%size);
		mensaje.tipo = PETIC;
		mensaje.origen = rank;
		MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE, (rank+1)%size, TAGCARGA, MPI_COMM_WORLD);
		*estadoAct = PASIVO; 
	}

	MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &hayMensajes, &status);
	while((pila->vacia() || hayMensajes) && *estadoAct != TERMINAR) {
		//Recibimos mensaje
		MPI_Probe(MPI_ANY_SOURCE, TAGCARGA, MPI_COMM_WORLD, &status);
		MPI_Recv(&mensaje, sizeof(Mensaje), MPI_BYTE, status.MPI_SOURCE, TAGCARGA, MPI_COMM_WORLD, &status);
		switch(mensaje.tipo) {
			case PETIC:
				//printf("Soy %i: recibo peticion de %i\n", rank, mensaje.origen);
				if(pila->vacia()) {
					if(mensaje.origen == rank) { //Nos vuelve nuestra petición
						//Detectar posible fin
						*estadoAct = PASIVO; 
						
						//Volver a enviar el mensaje de peticion
						mensaje.tipo = PETIC;
						mensaje.origen = rank;
						MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE, (rank+1)%size, TAGCARGA, MPI_COMM_WORLD);
						
						if(tenemosToken != NO) { //TENEMOS EL TOKEN
							if(rank == 0) tenemosToken = BLANCO;
							else if(MICOLOR == NEGRO) tenemosToken = NEGRO;
							mensaje.tipo = TOKEN;
							mensaje.colorToken = tenemosToken;
							MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE, (rank+1)%size, TAGCARGA, MPI_COMM_WORLD);
							MICOLOR = BLANCO;
							tenemosToken = NO;
						}
					}
					else { //reenviamos a (rank+1)%size
						//printf("Soy %i: no tengo nodos, reenvio la peticion de %i a %i\n", rank, mensaje.origen, (rank+1)%size);
						MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE, (rank+1)%size, TAGCARGA, MPI_COMM_WORLD); 
					}
				}
				else if(pila->tamanio() >= MINCEDER) {
					//Se los enviamos a mensaje.solicitante
					//printf("Soy %i: le voy a dar %i nodos (tengo %i) a %i\n", rank, pila->tamanio()/2, pila->tamanio(), mensaje.origen);
					mensajeTmp.numNodos = pila->tamanio()/2;
					
					vdatos = new int[mensajeTmp.numNodos * 2*NCIUDADES];
					
					for(int i = 0; i < mensajeTmp.numNodos; i++) {
						pila->pop(nodoTmp);
						memcpy(&vdatos[i*2*NCIUDADES], nodoTmp.datos, sizeof(int)*2*NCIUDADES);
					}
						
					mensajeTmp.tipo = NODOS;
					MPI_Send(&mensajeTmp, sizeof(Mensaje), MPI_BYTE, mensaje.origen, TAGCARGA, MPI_COMM_WORLD); 
					MPI_Send(vdatos, sizeof(int)*mensajeTmp.numNodos * 2*NCIUDADES, MPI_BYTE, mensaje.origen, TAGCARGA, MPI_COMM_WORLD);
					
					delete [] vdatos;
					
					if(rank < mensaje.origen)
						MICOLOR = NEGRO;
				}
				else {
					//reenviamos a (rank+1)%size
					//printf("*Soy %i: no tengo nodos suficientes, reenvio la peticion de %i a %i\n", rank, mensaje.origen, (rank+1)%size);
					MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE, (rank+1)%size, TAGCARGA, MPI_COMM_WORLD); 
				}
				
				break;
			case NODOS: //MENSAJE_TRABAJO
				//Recibimos los nodos
				//printf("Soy %i: voy a recibir %i nodos de %i\n", rank, mensaje.numNodos, status.MPI_SOURCE);
				vdatos = new int[mensaje.numNodos * 2*NCIUDADES];
				MPI_Recv(vdatos, sizeof(int)*mensaje.numNodos * 2*NCIUDADES, MPI_BYTE, status.MPI_SOURCE, TAGCARGA, MPI_COMM_WORLD, &status);
				for(int i = 0; i < mensaje.numNodos; i++) {
					memcpy(nodoTmp.datos, &vdatos[i*2*NCIUDADES], sizeof(int)*2*NCIUDADES);
					pila->push(nodoTmp);
				}
				delete [] vdatos;
				*estadoAct = ACTIVO;
				//printf("Soy %i: y ya he recibido %i nodos de %i\n", rank, mensaje.numNodos, status.MPI_SOURCE);
				break;
			case TOKEN:
				//printf("Soy %i: recibo token de %i con color %i y mi estado es %i\n", rank, status.MPI_SOURCE, mensaje.colorToken, *estadoAct);
				tenemosToken = mensaje.colorToken;
				if(*estadoAct == PASIVO) {
					if(rank == 0 && MICOLOR == BLANCO && tenemosToken == BLANCO) {
						//printf("Soy %i: he recibido el token correcto de fin, envio terminar a %i\n", rank, (rank+1)%size);
						//Sondear en busca de mensajes pendientes
						MPI_Iprobe(MPI_ANY_SOURCE, TAGCARGA, MPI_COMM_WORLD, &hayMensajes, &status);
						while(hayMensajes) {
							MPI_Recv(&mensaje, sizeof(Mensaje), MPI_BYTE, status.MPI_SOURCE, TAGCARGA, MPI_COMM_WORLD, &status);
							MPI_Iprobe(MPI_ANY_SOURCE, TAGCARGA, MPI_COMM_WORLD, &hayMensajes, &status);
						}
						//Enviamos Fin
						mensaje.tipo = FIN;
						MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE,(rank+1)%size, TAGCARGA, MPI_COMM_WORLD); 
						*estadoAct = TERMINAR;
					}
					else {
						///printf("Soy %i: recibo token pero no ha dado la vuelta o es negro, reenvio a %i\n", rank, (rank-1)%size);
						if(rank == 0) tenemosToken = BLANCO;
						else if(MICOLOR == NEGRO) tenemosToken = NEGRO;
						mensaje.tipo = TOKEN;
						mensaje.colorToken = tenemosToken;
						MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE,(rank+1)%size, TAGCARGA, MPI_COMM_WORLD); 
						MICOLOR = BLANCO;
						tenemosToken = NO;
					}
				}
				break;
			case FIN:
				//printf("Soy %i: recibo fin de %i\n", rank, status.MPI_SOURCE);
				*estadoAct = TERMINAR;
				if(rank < size-1) {
					mensaje.tipo = FIN;
					MPI_Send(&mensaje, sizeof(Mensaje), MPI_BYTE,(rank+1)%size, TAGCARGA, MPI_COMM_WORLD); 
				}
				break;
		}
		hayMensajes = false;
	}
}

void difusionCotaSuperior(int *U, bool *difundirLocal, bool *nuevaRecibida, int rank, int size) {
	MPI_Request request;
	MPI_Status status;
	int hayMensajes;
	int cotaRecibida;
	MensajeCota mensajeCota;
	static bool pendienteRetorno = false;

	
	MPI_Iprobe((rank-1+size)%size, TAGCOTA, MPI_COMM_WORLD, &hayMensajes, &status);
	while(hayMensajes) {
		MPI_Recv(&mensajeCota, sizeof(MensajeCota), MPI_BYTE, (rank-1+size)%size, TAGCOTA, MPI_COMM_WORLD, &status);
		
		if(mensajeCota.origen == rank) { //Era la nuestra, podemos volver a enviar
			pendienteRetorno = false;
		} else {
			cotaRecibida = mensajeCota.cota;
			if(cotaRecibida < *U) { //Actualizar Cota
				*U = cotaRecibida;
				*nuevaRecibida = true;
			}
			//Reenviamos la cota recibida
			MPI_Isend(&mensajeCota, sizeof(MensajeCota), MPI_BYTE, (rank+1)%size, TAGCOTA, MPI_COMM_WORLD, &request);
		}
		
		//Comprobamos si hay más mensajes
		MPI_Iprobe((rank-1+size)%size, TAGCOTA, MPI_COMM_WORLD, &hayMensajes, &status);
	}
	
	//Si tenemos que enviar, enviamos
	if(*difundirLocal && !pendienteRetorno) {
		mensajeCota.origen = rank;
		mensajeCota.cota = *U;
		MPI_Isend(&mensajeCota, sizeof(MensajeCota), MPI_BYTE, (rank+1)%size, TAGCOTA, MPI_COMM_WORLD, &request);
		pendienteRetorno = true;
		difundirLocal = false;
	}
}

