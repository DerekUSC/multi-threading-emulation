#include "defs.h"
#include "cs402.h"
#include "my402list.h"
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
pthread_t thread_A, thread_T, thread_S1,thread_S2,thread_SIG;
int num_tokens = 0;
int num_tokens_dropped = 0;
int num_tokens_total = 0;
int num;
int B;
int num_packets_total = 0;
int num_dropped = 0;
int num_removed = 0;
struct timeval time_start;
double t_Q1_total = 0;
double t_Q2_total = 0;
double t_ser1_total = 0;
double t_ser2_total = 0;
double t_interarrival_total = 0;
double t_sys_total = 0;
double t_sys_total2 = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t m_print = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv;
int flag = 0;
sigset_t set;
My402List *Q1;
My402List *Q2;
My402List *Q0;

struct mytoken{
	// char* B;
	char* r;
};

double detatime(struct timeval t1,struct timeval t2){
	long int deta_sec = t2.tv_sec - t1.tv_sec;
	long int deta_usec = t2.tv_usec - t1.tv_usec;
	long int deta_sec_msec = deta_sec*1000;
	double deta_usec_msec = (double)deta_usec/1000;
	return (double) deta_sec_msec + deta_usec_msec;
}

long int msecprint(struct timeval t){
	// gettimeofday(&t,NULL);
	long int deta_sec = t.tv_sec - time_start.tv_sec;
	long int deta_usec = t.tv_usec - time_start.tv_usec;
	long int deta_msec = deta_sec*1000 + deta_usec/1000;
	return deta_msec;
}
long int usecprint(struct timeval t){
	// gettimeofday(&t,NULL);
	long int deta_usec = t.tv_usec - time_start.tv_usec;
	return abs(deta_usec%1000);
}

int ifint(char* buf){
	int i = 0;
	for(i = 0;*(buf+i) != '\n';i++){
		if(*(buf+i) >= '0' && *(buf+i) <= '9'){
			continue;
		}
		else{
			return 0;
		}
	}
	return 1;
}

void *token_depositing(void * arg){
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
	struct mytoken* token = arg;
	double r = atof(token->r);
	double intervaltime_token = 1000/r;
	

	struct timeval t1;
	gettimeofday(&t1,NULL);
	struct timeval t2;
	double deta_t = (double)msecprint(t1);
	
	if(intervaltime_token > 10000){
		intervaltime_token = 10000;
	}
	while(1){
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0); 
		double val = (intervaltime_token - deta_t)*1000;
		usleep(val);
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

		pthread_mutex_lock(&m);
		gettimeofday(&t1,NULL);
		long int msec_t1 = msecprint(t1);
		long int usec_t1 = usecprint(t1);
		if(flag == 1){
			pthread_exit(0);
		}
		if(num_packets_total == num && My402ListEmpty(Q1) == 1){
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&m);
			pthread_exit(0);
		}
		
		num_tokens_total++;
		if(num_tokens < B){
			num_tokens++;
			fprintf(stdout,"%08ld.%03ldms: token t%d arrives, token bucket now has %d token\n",msec_t1,usec_t1,num_tokens_total,num_tokens);
		}
		else{
			fprintf(stdout,"%08ld.%03ldms: token t%d arrives, dropped\n",msec_t1,usec_t1,num_tokens_total);
			num_tokens_dropped++;
		}
		if(My402ListEmpty(Q1) == 0){
			struct object* packet_head =  My402ListFirst(Q1)->obj;
			if(num_tokens >= atoi(packet_head->P)){
				// pthread_mutex_lock(&m_print);
				struct timeval t_leaveQ1;
				gettimeofday(&t_leaveQ1,NULL);
				long int t_leaveQ1_msec = msecprint(t_leaveQ1);
				long int t_leaveQ1_usec = usecprint(t_leaveQ1);
				double t_Q1 = detatime(packet_head->t_enter,t_leaveQ1);
				t_Q1_total = t_Q1_total + t_Q1;

				fprintf(stdout,"%08ld.%03ldms: p%d leaves Q1, time in Q1 = %.3fms\n",t_leaveQ1_msec,t_leaveQ1_usec,packet_head->num_packet,t_Q1);
				// pthread_mutex_unlock(&m_print);
				// if(My402ListEmpty(Q2) == 1){
				// 	pthread_cond_broadcast(&cv);
				// }
				My402ListAppend(Q2,packet_head);
				// pthread_mutex_lock(&m_print);
				struct timeval t_enterQ2;
				gettimeofday(&t_enterQ2,NULL);
				long int t_enterQ2_msec = msecprint(t_enterQ2);
				long int t_enterQ2_usec = usecprint(t_enterQ2);
				packet_head->t_enterQ2 = t_enterQ2;
				
				fprintf(stdout,"%08ld.%03ldms: p%d enters Q2\n",t_enterQ2_msec,t_enterQ2_usec,packet_head->num_packet);	
				// pthread_mutex_unlock(&m_print);
				My402ListUnlink(Q1,My402ListFirst(Q1));
				num_tokens = num_tokens - atoi(packet_head->P);
			}
		}
		pthread_cond_broadcast(&cv);
		// pthread_mutex_lock(&m_print);
		gettimeofday(&t2,NULL);
		deta_t = detatime(t1,t2);
		// pthread_mutex_unlock(&m_print);
		pthread_mutex_unlock(&m);
		
	}
	
	return 0;

}

void *packet_arrival(void *arg){
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
	struct object *buf = arg;
	// struct object *packet = NULL;
	double intervaltime_pack;
	struct timeval t1;
	t1.tv_sec = time_start.tv_sec;
	t1.tv_usec = time_start.tv_usec;
	struct timeval t1_prev = t1;
	double deta_t = msecprint(t1);

	while(1){
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		struct object * packet = (struct object*)malloc(sizeof(struct object));
		*packet = *buf;		
		int length = 1024;
		packet->P = (char*)malloc(sizeof(char)*length);
		packet->lambda = (char*)malloc(sizeof(char)*length);
		packet->mu = (char*)malloc(sizeof(char)*length);
		packet->intervaltime_pack = (char*)malloc(sizeof(char)*length);
		packet->time_server = (char*)malloc(sizeof(char)*length);
		
		if(packet->m == 1){
			packet->intervaltime_pack = My402ListFirst(Q0)->obj->intervaltime_pack;
			packet->time_server = My402ListFirst(Q0)->obj->time_server;
			packet->P = My402ListFirst(Q0)->obj->P;
			intervaltime_pack = atof(packet->intervaltime_pack);
			// double time_server = atof(packet->time_server);
			My402ListUnlink(Q0,My402ListFirst(Q0));
		}
		if(packet->m == 0){

			strncpy(packet->P,buf->P,length);
			strncpy(packet->lambda,buf->lambda,length);
			strncpy(packet->mu,buf->mu,length);
			intervaltime_pack = 1000/atof(packet->lambda);
			if(intervaltime_pack > 10000){
				intervaltime_pack = 10000;
			}
		}
		
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
		double val = (intervaltime_pack - deta_t)*1000;
		usleep(val);

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
		pthread_mutex_lock(&m);

		gettimeofday(&t1,NULL);
		long int t1_msec = msecprint(t1);
		long int t1_usec = usecprint(t1);
		if(flag == 1){
			pthread_exit(0);
		}		
		num_packets_total++;
		packet->num_packet = num_packets_total;
		int P = atoi(packet->P);
		double t_interarrival = detatime(t1_prev,t1);
		t_interarrival_total = t_interarrival + t_interarrival_total;
		if(P > B){
			// pthread_mutex_lock(&m_print);
			fprintf(stdout,"%08ld.%03ldms: p%d arrives, needs %d tokens, intervaltime = %.3fms,dropped\n",t1_msec,t1_usec,num_packets_total,P,t_interarrival);
			num_dropped++;
			t1_prev = t1;
			// pthread_mutex_unlock(&m_print);
		}
		else{
			// pthread_mutex_lock(&m_print);
			fprintf(stdout,"%08ld.%03ldms: p%d arrives, needs %d tokens, intervaltime = %.3fms\n",t1_msec,t1_usec,num_packets_total,P,t_interarrival);
			t1_prev = t1;
			// pthread_mutex_unlock(&m_print);
			My402ListAppend(Q1,packet);
			// pthread_mutex_lock(&m_print);
			struct timeval t_enterQ1;
			gettimeofday(&t_enterQ1,NULL);
			packet->t_enter = t_enterQ1;
			long int t_enterQ1_msec = msecprint(t_enterQ1);
			long int t_enterQ1_usec = usecprint(t_enterQ1);
			fprintf(stdout,"%08ld.%03ldms: p%d enters Q1\n",t_enterQ1_msec,t_enterQ1_usec,packet->num_packet);
			// pthread_mutex_unlock(&m_print);
		}
		if(My402ListEmpty(Q1) != 1){
			struct object* packet_head;
			packet_head = My402ListFirst(Q1)->obj;
			int P_head = atoi(My402ListFirst(Q1)->obj->P);			
			if(P_head <= num_tokens )
			{
				// pthread_mutex_lock(&m_print);
				struct timeval t_leaveQ1;
				gettimeofday(&t_leaveQ1,NULL);
				double t_Q1 = detatime(packet_head->t_enter,t_leaveQ1);
				t_Q1_total = t_Q1 + t_Q1_total;
				long int t_leaveQ1_msec = msecprint(t_leaveQ1);
				long int t_leaveQ1_usec = usecprint(t_leaveQ1);
				fprintf(stdout,"%08ld.%03ldms: p%d leaves Q1, time in Q1 = %.3fms\n",t_leaveQ1_msec,t_leaveQ1_usec,packet_head->num_packet,t_Q1);
				// pthread_mutex_unlock(&m_print);
				// if(My402ListEmpty(Q2) == 1){
				// 	pthread_cond_broadcast(&cv);
				// }
				My402ListAppend(Q2,packet_head);
				// pthread_mutex_lock(&m_print);
				struct timeval t_enterQ2;
				gettimeofday(&t_enterQ2,NULL);
				packet_head->t_enterQ2 = t_enterQ2;
				long int t_enterQ2_msec = msecprint(t_enterQ2);
				long int t_enterQ2_usec = usecprint(t_enterQ2);
				fprintf(stdout,"%08ld.%03ldms: p%d enters Q2\n",t_enterQ2_msec,t_enterQ2_usec,packet_head->num_packet);		
				// pthread_mutex_unlock(&m_print);
				My402ListUnlink(Q1,My402ListFirst(Q1));
				num_tokens = num_tokens - P;
				// fprintf(stdin,"p%d leaves Q2",packet_head);
			}
		}
		if(num_packets_total == num){
			pthread_cond_broadcast(&cv);
			pthread_mutex_unlock(&m);
			break;
		}
		pthread_cond_broadcast(&cv);
		struct timeval t2;
		gettimeofday(&t2,NULL);
		deta_t = detatime(t1,t2);
		
		pthread_mutex_unlock(&m);

	}

	return 0;
}

void *server(void* arg){
	while(1){
		int* num_server_pointer = arg;
		int num_server = *num_server_pointer;
		pthread_mutex_lock(&m);
		
		if(flag == 1){
			pthread_mutex_unlock(&m);
			pthread_exit(0);
		}
		if(num_packets_total == num && My402ListEmpty(Q1) == 1 && My402ListEmpty(Q2) == 1){
			pthread_mutex_unlock(&m);
			break;
		}
		while(flag == 0 && My402ListEmpty(Q2) == 1 && !(num_packets_total == num && My402ListEmpty(Q1) == 1 && My402ListEmpty(Q2) == 1)){
			pthread_cond_wait(&cv,&m);
		}
		if(My402ListEmpty(Q2) != 1){

			struct object* packet_head = My402ListFirst(Q2)->obj;
			// pthread_mutex_lock(&m_print);
		 	struct timeval t_leaveQ2;
		 	gettimeofday(&t_leaveQ2,NULL);
		 	long int t_leaveQ2_msec = msecprint(t_leaveQ2);
		 	long int t_leaveQ1_usec = usecprint(t_leaveQ2);
		 	double t_Q2 = detatime(packet_head->t_enterQ2,t_leaveQ2);
		 	t_Q2_total = t_Q2_total + t_Q2;
		 	fprintf(stdout,"%08ld.%03ldms: p%d leaves Q2, time in Q2 = %.3fms\n",t_leaveQ2_msec, t_leaveQ1_usec,packet_head->num_packet,t_Q2);

			int time_server = 0;
			if(packet_head->m == 0){
				time_server = (int) 1000/atof(packet_head->mu);
				if(time_server > 10000){
					time_server = 10000;
				}
			}
			if(packet_head->m == 1){
				packet_head->time_server = My402ListFirst(Q2)->obj->time_server;
				time_server = atoi(packet_head->time_server);
			}
		
			struct timeval t_enterser;
			gettimeofday(&t_enterser,NULL);
			long int t_enterser_msec = msecprint(t_enterser);
			long int t_enterser_usec = usecprint(t_enterser);
			fprintf(stdout, "%08ld.%03ldms: p%d begins service at S%d, requesting %dms of service\n",t_enterser_msec,t_enterser_usec, packet_head->num_packet,num_server,time_server);
			My402ListUnlink(Q2,My402ListFirst(Q2));
			pthread_mutex_unlock(&m);

			usleep((time_server)*1000);
			
			pthread_mutex_lock(&m);
			struct timeval t_leave;
			gettimeofday(&t_leave,NULL);
			packet_head->t_leave = t_leave;
			long int t_leave_msec = msecprint(t_leave);
			long int t_leave_usec = usecprint(t_leave);
			double t_ser = detatime(t_enterser,t_leave);
			if(num_server == 1){
				t_ser1_total = t_ser + t_ser1_total;
			}
			if(num_server == 2){
				t_ser2_total = t_ser + t_ser2_total;
			}
			double t_sys = detatime(packet_head->t_enter,t_leave);
			t_sys_total = t_sys + t_sys_total;
			t_sys_total2 = t_sys_total2 + t_sys*t_sys;
			fprintf(stdout,"%08ld.%03ldms: p%d departs from S%d, service time = %.3fms,time in system = %.3fms\n",t_leave_msec,t_leave_usec,packet_head->num_packet,num_server,t_ser,t_sys);
		}
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&m);
	}
	return 0;

}

void* signal_catch(){
	int sig;
	sigwait(&set, &sig);
	pthread_mutex_lock(&m); 
	struct timeval t_sig;
	gettimeofday(&t_sig,NULL);
	long int t_sig_msec = msecprint(t_sig);
	long int t_sig_usec = msecprint(t_sig);   
	fprintf(stdout,"%08ld.%03ldms: SIGINT caught, no new packets or tokens will be allowed\n",t_sig_msec,t_sig_usec);
	flag = 1;
	pthread_cancel(thread_A);
	pthread_cancel(thread_T);
	pthread_cond_broadcast(&cv);
	if(My402ListEmpty(Q1) == 0){
		int num_members = Q1->num_members;
		int i = 1;
		for(i = 1;i <= num_members;i++){
			num_removed++;
			int packet_num = My402ListFirst(Q1)->obj->num_packet;
			struct timeval t_remove;
			gettimeofday(&t_remove,NULL);
			long int t_remove_msec = msecprint(t_remove);
			long int t_remove_usec = usecprint(t_remove);
			fprintf(stdout,"%08ld.%03ldms: p%d removed from Q1\n",t_remove_msec,t_remove_usec,packet_num);
			My402ListUnlink(Q1,My402ListFirst(Q1));
		}
	}
	if(My402ListEmpty(Q2) == 0){
		int num_members = Q2->num_members;
		int i = 1;
		for(i = 1;i <= num_members;i++){
			num_removed++;
			int packet_num = My402ListFirst(Q2)->obj->num_packet;
			struct timeval t_remove;
			gettimeofday(&t_remove,NULL);
			long int t_remove_msec = msecprint(t_remove);
			long int t_remove_usec = usecprint(t_remove);
			fprintf(stdout,"%08ld.%03ldms: p%d removed from Q2\n",t_remove_msec,t_remove_usec,packet_num);
			My402ListUnlink(Q2,My402ListFirst(Q2));
		}
	}
	pthread_mutex_unlock(&m);
	
	return 0;
}

void addnode(My402List *Q0,char *buf){
    const int length = 1024;
    struct object *packet = (struct object*)malloc(sizeof(struct object));
    packet->P = (char*)malloc(sizeof(char)*length);
    packet->intervaltime_pack = (char*)malloc(sizeof(char)*length);
    packet->time_server = (char*)malloc(sizeof(char)*length);

    strncpy(packet->intervaltime_pack,strtok(buf," '\t'"),length);
    char* P_add = strtok(NULL," '\t'");
    strncpy(packet->P,P_add,length);
    char* time_server_char = strtok(NULL," '\t'");
    strncpy(packet->time_server,time_server_char,length);
   
    My402ListAppend(Q0,packet);
    return;
}

void processfile(char* filename)
{
    FILE *fp;
    fp = fopen(filename,"r");
    if(fp == NULL){
    	fprintf(stderr,"file access is defied\n");
    	exit(0);
    }
    char buf[1026];
    int count = 0;
    while(1){
        count++;
        if(fgets(buf,sizeof(buf),fp) == NULL){
            if(strlen(buf) > 1024){
                fprintf(stderr,"error: length of line exceeds\n");
                fclose(fp);
                exit(0);
            }
            fclose(fp);
            break;
        }
        else{                  
            if(count == 1){
            	// int temp;
            	if(ifint(buf) == 0){
            		fprintf(stderr,"malformed input - line 1 is not just a number\n");
            		exit(0);
            	}
            	strtok(buf," '\t'");
            	num = atoi(buf);
            }
            else{
            	addnode(Q0,buf);
            }
            
        }     
    }

    return;
}


void output(){
	struct timeval time_end;

	pthread_mutex_lock(&m);
	gettimeofday(&time_end,NULL);
	pthread_mutex_unlock(&m);

	long int time_end_msec = msecprint(time_end);
	long int time_end_usec = usecprint(time_end);
	fprintf(stdout,"%08ld.%03ldms: emulation ends\n",time_end_msec,time_end_usec);
	fprintf(stdout,"\nStatistics: \n");
	
	double avetime_interarrival = t_interarrival_total/num_packets_total;
	double avetime_ser = (t_ser1_total + t_ser2_total)/(num_packets_total - num_dropped - num_removed);
	if(num_packets_total == 0){
		fprintf(stdout,"\n 	average packet inter-arrival time = N/A: no packet arrived at this facility\n");
	}
	else{
		fprintf(stdout,"\n 	average packet inter-arrival time = %.6g\n",avetime_interarrival/1000);
	}
	if(num_packets_total - num_dropped - num_removed == 0){
		fprintf(stdout,"	average packet service time = N/A: no completed packets\n");
	}
	else{
		fprintf(stdout,"	average packet service time = %.6g\n",avetime_ser/1000);
	}
	double t_simu = detatime(time_start,time_end);
	double avenum_Q1 = t_Q1_total/t_simu;
	double avenum_Q2 = t_Q2_total/t_simu;
	double avenum_S1 = t_ser1_total/t_simu;
	double avenum_S2 = t_ser2_total/t_simu;
	fprintf(stdout,"\n 	average number of packets in Q1 = %.6g\n",avenum_Q1);
	fprintf(stdout,"	average number of packets in Q2 = %.6g\n",avenum_Q2);
	fprintf(stdout,"	average number of packets in S1 = %.6g\n",avenum_S1);
	fprintf(stdout,"	average number of packets in S2 = %.6g\n",avenum_S2);

	double avetime_sys = t_sys_total/(num_packets_total - num_removed - num_dropped);
	double avetime_sys2 = t_sys_total2/(num_packets_total - num_removed - num_dropped);
	double deta = sqrt(avetime_sys2 - avetime_sys*avetime_sys);
	if(num_packets_total - num_removed - num_dropped == 0){
		fprintf(stdout,"\n 	average time a packet spent in system = N/A:no completed packets\n");
	}
	else{
		fprintf(stdout,"\n 	average time a packet spent in system = %.6g\n",avetime_sys/1000);
	}
	if(num_packets_total - num_removed - num_dropped == 0){
		fprintf(stdout,"	standard deviation for time spent in system = N/A: no completed packets\n");
	}
	else{
		fprintf(stdout,"	standard deviation for time spent in system = %.6g\n",deta/1000);
	}
	fprintf(stdout,"\n 	token drop probability = %.6g\n",(double)num_tokens_dropped/(double)num_tokens_total);
	if(num_packets_total == 0){
		fprintf(stdout," 	packet drop probability = N/A: no packet arrived\n");
	}
	else{
		fprintf(stdout," 	packet drop probability = %.6g\n",(double)num_dropped/(double)num_packets_total);
	}
	
	return;

}

int main(int argc, char *argv[]){
	// const int length = 1024;
	Q1 = (My402List*)malloc(sizeof(My402List));
	Q2 = (My402List*)malloc(sizeof(My402List));
	Q0 = (My402List*)malloc(sizeof(My402List));
	My402ListInit(Q0);
	My402ListInit(Q1);
	My402ListInit(Q2);
	sigemptyset(&set);  
	sigaddset(&set,SIGINT);  
	sigprocmask(SIG_BLOCK,&set,0);
	// char* filename,r,mu,lambda,B_char,P,num_char;
	char* lambda = "1";
	char* filename = NULL;
	char* r = "1.5";
	char* mu = "0.35";
	char* B_char = "10";
	char* P = "3";
	char* num_char = "20";
	if(argc > 1){
		if(strchr(argv[1],'-') == NULL){
			fprintf(stderr,"malformed command: no '-' in your input\n");
			exit(0);
		}
	}
	int i = 1;
	for(i = 1;i < argc;i++){
		if(strcmp(argv[i],"-t") == 0){
			filename = argv[i+1];
			if( access( filename, F_OK ) == -1 ) {
    			fprintf(stderr,"input file %s does not exist\n",filename);
    			exit(0);
			} 
			i++;
			continue;
		}
		if(strcmp(argv[i],"-lambda") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -lambda\n");
				exit(0);
			}
			lambda = argv[i+1];
			continue;
		}
		if(strcmp(argv[i],"-r") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -r\n");
				exit(0);
			}
			r = argv[i+1];
			continue;
		}
		if(strcmp(argv[i],"-mu") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -mu\n");
				exit(0);
			}
			mu = argv[i+1];
			continue;
		}
		if(strcmp(argv[i],"-B") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -B\n");
				exit(0);
			}
			B_char = argv[i+1];
			continue;
		}
		if(strcmp(argv[i],"-P") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -P\n");
				exit(0);
			}
			P = argv[i+1];
			continue;
		}
		if(strcmp(argv[i],"-n") == 0){
			if(i + 1 >= argc || strchr(argv[i+1],'-') != NULL){
				fprintf(stderr,"malformed command: there must be a number behind -n\n");
				exit(0);
			}
			num_char = argv[i+1];
			continue;
		}
		if(strchr(argv[i],'-') != NULL){
			fprintf(stderr,"malformed command: the character behind '-' is not the required input\n");
			exit(0);
		}				
	}
	
	const int length = 1024;
	struct mytoken * token = (struct mytoken*)malloc(sizeof(struct mytoken));
	token->r = (char*)malloc(sizeof(char)*length);
	token->r = r;
	B = atoi(B_char);
	struct object * packet = (struct object*)malloc(sizeof(struct object));
	
	if(filename == NULL){

		packet->P = (char*)malloc(sizeof(char)*length);
		packet->lambda = (char*)malloc(sizeof(char)*length);
		packet->mu = (char*)malloc(sizeof(char)*length);
	
		packet->P = P;
		packet->lambda = lambda; 
		packet->mu = mu;
		num = atoi(num_char);
		packet->m = 0;
		fprintf(stdout,"Emulation Parameters:\n");
		fprintf(stdout,"	number to arrive = %d\n",num);
		fprintf(stdout,"	lambda = %s\n",lambda);
		fprintf(stdout,"	mu = %s\n",mu);
		fprintf(stdout,"	r = %s\n",r);
		fprintf(stdout,"	B = %d\n",B);
		fprintf(stdout," 	P = %s\n",P);
	}
	else{
		packet->m = 1;
		processfile(filename);
		fprintf(stdout,"	r = %s\n",r);
		fprintf(stdout,"	B = %d\n",B);
		fprintf(stdout,"	tsfile = %s\n",filename);	
	}
	int k = 1;
	int j = 2;
	
	gettimeofday(&time_start,NULL);
	fprintf(stdout,"\n%08d.%03dms: emulation begins\n",0,0);
	pthread_create(&thread_A,0,packet_arrival,packet);
	pthread_create(&thread_T,0,token_depositing,token);
	pthread_create(&thread_S1,0,server,&k);
	pthread_create(&thread_S2,0,server,&j);
	pthread_create(&thread_SIG,0,signal_catch,0);

	pthread_join(thread_A,0);
	pthread_join(thread_S1,0);
	pthread_join(thread_S2,0);

	output();
	return 0;
}
