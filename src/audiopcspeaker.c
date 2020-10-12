/*
    ZEsarUX  ZX Second-Emulator And Released for UniX 
    Copyright (C) 2013 Cesar Hernandez Bano

    This file is part of ZEsarUX.

    ZEsarUX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#include <sys/io.h>
#include <sys/time.h>



#include "audiopcspeaker.h"
#include "cpu.h"
#include "audio.h"
#include "compileoptions.h"
#include "debug.h"
#include "settings.h"

#ifdef USE_PTHREADS
#include <pthread.h>

     pthread_t audiopcspeaker_thread1;

#endif

//int ptr_audiopcspeaker;

//unsigned int requested, ioctl_format, ioctl_channels, ioctl_rate;

//#define BASE_SOUND_FRAG_PWR     6

//static char const * const default_device = "/dev/input/by-path/platform-pcspkr-event-spkr";

int audiopcspeaker_init(void)
{

	//audio_driver_accepts_stereo.v=1;



    debug_printf (VERBOSE_INFO,"Init PC Speaker Audio Driver");


	//Metodo alternativo era usando el device default_device y enviando eventos, pero eso solo gestiona frecuencias
                //ptr_audiopcspeaker=open(default_device,O_WRONLY);


	//Pedir "permiso" para usar puerto pc speaker
	if (ioperm(0x61,1,1)) {
		debug_printf(VERBOSE_ERR,"Error asking permissions on speaker port");
		return 1;
	}




	//Esto debe estar al final, para que funcione correctamente desde menu, cuando se selecciona un driver, y no va, que pueda volver al anterior
	audio_set_driver_name("pcspeaker");

	return 0;

}



int audiopcspeaker_thread_finish(void)
{

	if (audiopcspeaker_thread1!=0) {
		debug_printf (VERBOSE_DEBUG,"Ending audiopcspeaker thread");
		int s=pthread_cancel(audiopcspeaker_thread1);
		if (s != 0) debug_printf (VERBOSE_DEBUG,"Error cancelling pthread pcspeaker");

		audiopcspeaker_thread1=0;
	}

	//Pausa de 0.1 segundo
	 usleep(100000);
	

	return 0;

}

void audiopcspeaker_end(void)
{
	debug_printf (VERBOSE_INFO,"Ending pcspeaker audio driver");
	audiopcspeaker_thread_finish();
	audio_playing.v=0;

	//stop_beep();

	//close(ptr_audiopcspeaker);
}







/* no usados estos
void beep_freq(int frequency)
{
	struct input_event e = { 0 };
	e.type = EV_SND;
	e.code = SND_TONE;
	e.value = frequency; //freq;
	write(ptr_audiopcspeaker, &e, sizeof(e));
}

void start_beep(void)
{
  beep_freq(10);
}

void stop_beep(void)
{
beep_freq(0);
}
*/


struct timeval audiopcspeakertimer_antes, audiopcspeakertimer_ahora;

//Para calcular tiempos funciones. Iniciar contador antes
void audiopcspeakertiempo_inicial(void)
{

        gettimeofday(&audiopcspeakertimer_antes, NULL);

}

//Para calcular tiempos funciones. Contar contador despues e imprimir tiempo por pantalla
int audiopcspeakertiempo_final(void)
{

        long audiopcspeakertimer_mtime, audiopcspeakertimer_seconds, audiopcspeakertimer_useconds;    

        gettimeofday(&audiopcspeakertimer_ahora, NULL);

        audiopcspeakertimer_seconds  = audiopcspeakertimer_ahora.tv_sec  - audiopcspeakertimer_antes.tv_sec;
        audiopcspeakertimer_useconds = audiopcspeakertimer_ahora.tv_usec - audiopcspeakertimer_antes.tv_usec;

        audiopcspeakertimer_mtime = ((audiopcspeakertimer_seconds) * 1000 + audiopcspeakertimer_useconds/1000.0) + 0.5;

        //printf("Elapsed time: %ld milliseconds\n\r", audiopcspeakertimer_mtime);

	return audiopcspeakertimer_mtime;
}



char *buffer_playback_pcspeaker;

char last_audio_sample=0;

int audiopcspeaker_esperando_frame=0;

int audiopcspeaker_calibrando_tiempo_espera=0;

void audiopcspeaker_calibrate_tiempo_espera(void)
{
	audio_playing.v=0;

	//Esperar que se vaya a bucle de espera
	usleep(1000000);

	//Y activar calibracion

	audiopcspeaker_calibrando_tiempo_espera=1;

	//Empezamos con wait 0
	audiopcspeaker_tiempo_espera=0;

	audio_playing.v=1;
}


//Ultimo valor.
z80_byte bit_anterior_speaker=0;

void *audiopcspeaker_enviar_audio(void *nada)
{


	while (1) {

		audiopcspeaker_esperando_frame=1;


		//Establecer el buffer de reproduccion
		buffer_playback_pcspeaker=audio_buffer_playback;

		//enviamos siguiente sonido avisando de interrupcion a cpu
		//interrupt_finish_sound.v=1;



		int len=AUDIO_BUFFER_SIZE;

		int ofs=0;

		audiopcspeakertiempo_inicial();


		//Valor de referencia
		/*
Port 61h controls how the speaker will operate as follows:

Bit 0    Effect
-----------------------------------------------------------------
  0      The state of the speaker will follow bit 1 of port 61h
  1      The speaker will be connected to PIT channel 2, bit 1 is
         used as switch ie 0 = not connected, 1 = connected.		
		*/
		z80_byte valor_puerto_original=inb(0x61);
		valor_puerto_original &=(255-2-1);



		z80_byte bit_final_speaker;
		for (;len>0;len--) {
			
			char current_audio_sample=buffer_playback_pcspeaker[ofs];
			
			//Si valor actual es mayor, enviar 1
			if (current_audio_sample>last_audio_sample) {
				// altavoz a 1

				bit_final_speaker=2;
			}

			//Si es menor , enviar 0
			else if (current_audio_sample<last_audio_sample) {
				// altavoz a 0

				bit_final_speaker=0;

			}

			//Si es igual, dejar el mismo valor anterior
			else {
				bit_final_speaker=bit_anterior_speaker;
			}

			last_audio_sample=current_audio_sample;

			int tiempo_espera=audiopcspeaker_tiempo_espera;

			//Si cambia el altavoz
			if (bit_anterior_speaker!=bit_final_speaker) {
				outb(valor_puerto_original | bit_final_speaker,0x61);
				tiempo_espera--; //se supone que el out tarda 1 microsegundo
			}

			if (tiempo_espera>0) usleep(tiempo_espera);

			bit_anterior_speaker=bit_final_speaker;

			ofs++;
			//stereo. Pasamos del otro canal directamente
			ofs++;
		}
         
		int tiempo_pasado_ms=audiopcspeakertiempo_final();

		if (audiopcspeaker_calibrando_tiempo_espera) {
			int ideal_time=20*FRAMES_VECES_BUFFER_AUDIO; //20 ms*frames
			debug_printf(VERBOSE_INFO,"Calibrating. Wait time: %d Elapsed time: %d ms ideal: %d ms",audiopcspeaker_tiempo_espera,tiempo_pasado_ms,20*FRAMES_VECES_BUFFER_AUDIO);

			//Si se pasa
			if (tiempo_pasado_ms>ideal_time) {
				//Nos quedamos con valor anterior
				audiopcspeaker_tiempo_espera--;
				if (audiopcspeaker_tiempo_espera<0) audiopcspeaker_tiempo_espera=0;

				audiopcspeaker_calibrando_tiempo_espera=0;

				debug_printf(VERBOSE_INFO,"End calibration. Wait time parameter: %d",audiopcspeaker_tiempo_espera);

			}

			else {
				//Seguimos probando
				audiopcspeaker_tiempo_espera++;
				if (audiopcspeaker_tiempo_espera>64) {
					audiopcspeaker_tiempo_espera=64;
					audiopcspeaker_calibrando_tiempo_espera=0;

					debug_printf(VERBOSE_INFO,"End calibration, reached limit. Wait time parameter: %d",audiopcspeaker_tiempo_espera);
				}

			}
		}

		while (audio_playing.v==0 || silence_detection_counter==SILENCE_DETECTION_MAX) {
				//1 ms
				usleep(1000);
		}

		//Esperamos a que llegue el siguiente frame de sonido , si es que no ha llegado ya
		//TODO: esto deberia ser una variable atomica, pero la probabilidad que se modifique 
		//en esta funcion y en audiopcspeaker_send_frame a la vez es casi nula
		//ademas si se pierde un frame, entrara el siguiente
		while (audiopcspeaker_esperando_frame) {
			//100 microsegundos
			usleep(100);
		}

	}


	//Para evitar warnings al compilar de unused parameter ‘nada’ [-Wunused-parameter]
	nada=0;
	nada++;


	return NULL;
} 

pthread_t audiopcspeaker_thread1=0;

void audiopcspeaker_send_frame(char *buffer)
{


	if (audio_playing.v==0) {
			//Volvemos a activar pthread
			buffer_playback_pcspeaker=buffer;
			audio_playing.v=1;
	}


	if (audiopcspeaker_thread1==0) {
		buffer_playback_pcspeaker=buffer;
     	if (pthread_create( &audiopcspeaker_thread1, NULL, &audiopcspeaker_enviar_audio, NULL) ) {
                cpu_panic("Can not create audiopcspeaker pthread");
        }      
	}

	audiopcspeaker_esperando_frame=0;
	
}



void audiopcspeaker_get_buffer_info (int *buffer_size,int *current_size)
{
  *buffer_size=AUDIO_BUFFER_SIZE*2; //*2 porque es stereo
  *current_size=0;
}

