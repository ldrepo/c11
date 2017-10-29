/*============================================================================
 * Licencia:
 * Autor:
 * Fecha:
 *===========================================================================*/

/*==================[inlcusiones]============================================*/
//#include "program.h"   // <= su propio archivo de cabecera (opcional)
#include "sapi.h"        // <= Biblioteca sAPI

/*==================[definiciones y macros]==================================*/
typedef enum{	MODO1_SECUENCIA,
				MODO2_INTERMITENTE,
				MODO_CAPTURA_TIEMPO,
				MODO_CONFIGURACION
			} estadoMEF;

#define CONFIGURACION_UART_BAUDRATE				(115200)
#define LARGO_SECUENCIA                         (4)
#define T_TICK_RATE_MS                         	(1)
#define T_LEDS_SECUENCIA_INICIAL_MS				(200)
#define T_LED_INTERMITENTE_MS					(300)
#define T_ANTIRREBOTE_MS						(40)

/*==================[definiciones de datos internos]=========================*/
CONSOLE_PRINT_ENABLE

volatile bool_t flagTick;
tick_t tiempoSecuencia;

/*==================[definiciones de datos externos]=========================*/

/*==================[declaraciones de funciones internas]====================*/
bool_t 		myTickHook				( void *ptr );
void 		Rutina_CargoSecuencia	(uint8_t *ptSecuencia, uint8_t largoSecuencia);
uint8_t 	Rutina_Secuencia		(uint8_t *ptSecuencia, uint8_t largoSecuencia);
uint8_t 	Rutina_Intermitente		(void);
void 		PrendeLed				(uint8_t codigo);
tick_t	 	Rutina_CapturaTiempo	(void);
/*==================[declaraciones de funciones externas]====================*/

/*==================[funcion principal]======================================*/

// FUNCION PRINCIPAL, PUNTO DE ENTRADA AL PROGRAMA LUEGO DE ENCENDIDO O RESET.
int main( void ){
	estadoMEF  estadoActual;
	uint8_t secuencia[LARGO_SECUENCIA]={'a','1','2','3'};
	uint8_t rta;

	// ---------- CONFIGURACIONES ------------------------------

	// Inicializar y configurar la plataforma
	boardConfig();

	// Inicializar UART_USB como salida de consola
	consolePrintConfigUart( UART_USB, 115200 );

	// INICIALIZO SYSTICK
	flagTick = OFF;
	tickConfig( T_TICK_RATE_MS, myTickHook );

	// INICIALIZO MAQUINA DE ESTADOS
	tiempoSecuencia = T_LEDS_SECUENCIA_INICIAL_MS;		//Inicializa tiempo de secuencia
	estadoActual = MODO1_SECUENCIA;
	consolePrintString("\r\n-- Modo1 Secuencia --");
	// ---------- REPETIR POR SIEMPRE --------------------------
	while( TRUE )
	{
		if(ON == flagTick) {
			flagTick = OFF;

			/*
			 * Maquina de estado de mi programa principal
			 */
			switch (estadoActual) {
				case MODO_CONFIGURACION:
					Rutina_CargoSecuencia(secuencia,LARGO_SECUENCIA); // bloqueante
					consolePrintString("\r\n-- Modo1 Secuencia --");
					estadoActual = MODO1_SECUENCIA;
					break;

				case MODO1_SECUENCIA:
					rta = Rutina_Secuencia(secuencia,LARGO_SECUENCIA); // devuelve 0: nada, 1: si tec4, 2: llega f, 3: si tec3
					if(1 == rta) {
						consolePrintString("\r\n-- Modo2 Intermitente --");
						estadoActual = MODO2_INTERMITENTE;
					} else if(2 == rta) {
						consolePrintString("\r\n-- Modo Configuración --");
						estadoActual = MODO_CONFIGURACION;
					} else if(3 == rta) {
						estadoActual = MODO_CAPTURA_TIEMPO;
					}
					break;

				case MODO_CAPTURA_TIEMPO:
					tiempoSecuencia = Rutina_CapturaTiempo();
					estadoActual = MODO1_SECUENCIA;
					break;

				case MODO2_INTERMITENTE:
					rta = Rutina_Intermitente(); // devuelve 0: nada, 1: si tec1, 2: llega f
					if(1 == rta) {
						consolePrintString("\r\n-- Modo1 Secuencia --");
						estadoActual = MODO1_SECUENCIA;
					} else if(2 == rta) {
						consolePrintString("\r\n-- Modo Configuración --");
						estadoActual = MODO_CONFIGURACION;
					}
					break;

				default:
					estadoActual = MODO1_SECUENCIA;
					break;
			}

		}
   }

   // NO DEBE LLEGAR NUNCA AQUI, debido a que a este programa se ejecuta
   // directamenteno sobre un microcontroladore y no es llamado por ningun
   // Sistema Operativo, como en el caso de un programa para PC.
   return 0;
}

/*==================[definiciones de funciones internas]=====================*/

bool_t myTickHook( void *ptr ){
	flagTick = ON;
	return 1;
}

/**
 * @brief	Toma nueva secuencia desde la UART
 * @param	*ptSecuencia		: dirección de destino de la secuencia recibida
 * @param	largoSecuencia  	: longitud de la secuencia a recibir
 * @return	void
 * @note
 */
void Rutina_CargoSecuencia(uint8_t *ptSecuencia, uint8_t largoSecuencia){
	uint8_t dato;
	int n;
	consolePrintString("\r\nIngrese nueva secuencia de 4 pasos.");
	consolePrintString("\r\nLas opciones a:LEDB, 1:LED1, 2:LED2, 3:LED3]: ");
	for(n = 0; n < largoSecuencia  ; n++ ){
		do{
			while(!uartReadByte( UART_USB, &dato));	//Espera la recepción de 1 byte
		}while(dato != 'a' && dato != '1' && dato != '2' && dato != '3');
		uartWriteByte( UART_USB, dato);				//Eco
        *ptSecuencia++ = dato;
	}
}

/**
 * @brief	Reproduce secuencia y verifica pulsadores
 * @param	*ptSecuencia		: dirección de la secuencia para jugar
 * @param	largoSecuencia  	: longitud de la secuencia
 * @return	0: nada, 1: si tec4, 2: llega f, 3: si tec3
 * @note
 */
uint8_t Rutina_Secuencia(uint8_t *ptSecuencia, uint8_t largoSecuencia){
	uint8_t *ptSecuenciaTemp, dato;
	int n;
	tick_t tickTemp;

	while(1){
		ptSecuenciaTemp = ptSecuencia;				//Apunta al principio
		for(n = 0; n <largoSecuencia ; n++ ){					//Reproduce Secuencia
			PrendeLed(*ptSecuenciaTemp);			//Prende el led
			ptSecuenciaTemp++;						//Avanza la secuencia
			tickTemp = tickRead() + tiempoSecuencia;//Inicia demora no bloqueante
			while (tickRead() < tickTemp){			//Mientras espera el tiempo de secuencia
				//Chequea TEC3
				if ( !gpioRead(TEC3) ){
					return (3);
				}
				//Chequea TEC4
				if ( !gpioRead(TEC4) ){
					return (1);
				}
				//Chequea UART
				if(uartReadByte( UART_USB, &dato)){
					if('f'==dato || 'F'==dato){
						return (2);
					}
				}
			}
		}
	}
	return 0;
}

/**
 * @brief	Blinkea los led en forma simultanea
 * @param	void
 * @return 	devuelve 0: nada, 1: si tec1, 2: llega f
 * @note
 */
uint8_t Rutina_Intermitente(void){
	uint8_t dato;
	tick_t tickTemp;
	PrendeLed(0);					//Apaga todo
	while(1){
		//Toglea Leds
		gpioToggle( LEDB );
		gpioToggle( LED1 );
		gpioToggle( LED2 );
		gpioToggle( LED3 );

		tickTemp = tickRead() + T_LED_INTERMITENTE_MS;//Inicia demora no bloqueante
		while (tickRead() < tickTemp){			//Mientras espera el tiempo de secuencia
			//Chequea TEC1
			if ( !gpioRead(TEC1) ){
				delay(T_ANTIRREBOTE_MS);
				return (1);
			}
			//Chequea UART
			if(uartReadByte( UART_USB, &dato)){
				if('f'==dato || 'F'==dato){
					return (2);
				}
			}
		}
	}
	return 0;
}

/**
 * @brief	Prende el led que corresponde según el código proporcionado
 * @param	codigo: código actual de la secuencia, 0 para apagar todo, t prende tod
 * @return	void
 * @note
 */
void PrendeLed(uint8_t codigo){
	switch(codigo){
	case 'a':
		gpioWrite( LEDB , ON );
		gpioWrite( LED1 , OFF );
		gpioWrite( LED2 , OFF );
		gpioWrite( LED3 , OFF );
		break;
	case '1':
		gpioWrite( LEDB , OFF );
		gpioWrite( LED1 , ON );
		gpioWrite( LED2 , OFF );
		gpioWrite( LED3 , OFF );
		break;
	case '2':
		gpioWrite( LEDB , OFF );
		gpioWrite( LED1 , OFF );
		gpioWrite( LED2 , ON );
		gpioWrite( LED3 , OFF );
		break;
	case '3':
		gpioWrite( LEDB , OFF );
		gpioWrite( LED1 , OFF );
		gpioWrite( LED2 , OFF );
		gpioWrite( LED3 , ON );
		break;
	case 't':
		gpioWrite( LEDB , ON );
		gpioWrite( LED1 , ON );
		gpioWrite( LED2 , ON );
		gpioWrite( LED3 , ON );
		break;
	case 0:
		gpioWrite( LEDB , OFF );
		gpioWrite( LED1 , OFF );
		gpioWrite( LED2 , OFF );
		gpioWrite( LED3 , OFF );
		break;
	}

}

/**
 * @brief	Toma el tiempo que se mantiene presionada TEC3
 * @param	void
 * @return	cantidad de ticks de tecla presionada
 * @note
 */tick_t Rutina_CapturaTiempo(void){
	tick_t tickTemp = tickRead();	//Toma tiempo inicial
	while( !gpioRead(TEC3) ); 		//Espera que suelte tecla 3
	tickTemp = tickRead()-tickTemp; //Calcula tiempo transcurrido
	delay(T_ANTIRREBOTE_MS);
	return tickTemp;
}
/*==================[definiciones de funciones externas]=====================*/

/*==================[fin del archivo]========================================*/
