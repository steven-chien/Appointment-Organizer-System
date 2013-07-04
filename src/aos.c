/************************************************************************/
/* Copyright 2013 Steven Chien						*/
/* This program is free software: you can redistribute it and/or modify	*/
/* it under the terms of the GNU General Public License as published by	*/
/* the Free Software Foundation, either version 3 of the License, or	*/
/* (at your option) any later version.					*/
/*									*/
/* This program is distributed in the hope that it will be useful,	*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*/
/* GNU General Public License for more details.				*/
/* You should have received a copy of the GNU General Public License	*/
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.*/
/************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>

int P2C[10][2];
int C2P[10][2];
//development purpose
int child_no = 3;
int priority = 2;
char absentee[20];
pid_t parent_pid;

//create type event
typedef struct Event {
	//define struct event
	int priority;			//identify event type
	time_t start_time;		//start time of event
	time_t end_time;		//end time of event
	char usr[20][20];		//name of participant
	int parti_no;			//number of participants
	struct Event *next;		//pointer to next event
} event;

//create type user
typedef struct User {
	//define struct user
	char name[20];			//name of user
	pid_t process_id;		//process ID of user
	int event_count;		//number of events engaged in
	int rejection_count;		//number of events being rejected
	int id;				//(array) id of user
	event *start, *end;		//head and tail of link list of events
	event *rej_start, *rej_end;	//head and tail of link list of rejected events
} user;

//int availibility[10] = { 1 };

//function to extract time from command
void parse_time(char *command, time_t *start_time, time_t *end_time) {
	//define time struction for start and end
	struct tm start;
	struct tm end;
	int i;
	char *token;
	//tokenize string to the part where time data is descripted
	token = strtok(command, " :-.");
	token = strtok(NULL, " :-.");

	//extract year
	token = strtok(NULL, " :-.");
	int year = atoi(token);
	////printf("DEBUG!: eventAdd(): year %d\n", year);
	start.tm_year = year - 1900;

	//extract month
	token = strtok(NULL, " :-.");
	int month = atoi(token);
	////printf("DEBUG!: eventAdd(): month %d\n", month);
	start.tm_mon = month - 1;

	//extract day
	token = strtok(NULL, " :-.");
	int day = atoi(token);
	////printf("DEBUG!: eventAdd(): day %d\n", day);
	start.tm_mday = day;

	//extract hour
	token = strtok(NULL, " :-.");
	int hour = atoi(token);
	////printf("DEBUG!: eventAdd(): hour %d\n", hour);
	start.tm_hour = hour;

	//extract minute
	token = strtok(NULL, " :-.");
	int minute = atoi(token);
	////printf("DEBUG!: eventAdd(): minute %d\n", minute);
	start.tm_min = minute;

	//second
	start.tm_sec = 0;

	//create end_time as a duplicate of start
	memcpy(&end, &start, sizeof(struct tm));

	//adjust end time
	//adjust ending hour
	token = strtok(NULL, " :-.");
	int duration_hour = atoi(token);
	////printf("DEBUG!: parse_time(): duration hour = %d\n", duration_hour);
	end.tm_hour = end.tm_hour + duration_hour;

	//adjust ending min
	token = strtok(NULL, " :-.");
	int duration_min = atoi(token);
	//printf("parse_time(): duration min = %d\n", duration_min);
	end.tm_min = end.tm_min + ((float)duration_min/10*60);

	//finialize times to time_t format
	*start_time = mktime(&start);
	*end_time = mktime(&end);
	//printf("her end time is %s\n", ctime(end_time));

}

//check if requested event colid with events in list; return 0 = no overlap, 1 = overlap
int check_overlap(user *member, time_t _start, time_t _end) {
	//debug!
	//printf("DEBUG!: child %d: check_overlap(): given start %d, end %d\n", member->id, _start, _end);

	//compute duration of requested event
	double duration = difftime(_end, _start);
	//printf("DEBUG!: child %d: check_overlap(): duration of event is %f\n", member->id, duration/60.0/60.0);

	//create new node to process event
	event *p = (event*)malloc(sizeof(event));
	p = member->start;
	event *r = (event*)malloc(sizeof(event));

	if(p==NULL ||(_end)<=(p->start_time)) {
		//check if list is empty, return 0 if so
		//printf("DEBUG!: child %d: check_overlap(): list is empty, no overlap\n", member->id);
		return 0;
	} else {
		//look for right insertion position if list is not empty
		for(p=member->start, r=p->next; p!=NULL; p=p->next, r=p->next) {		//find position, p = one item before the positon

			if(p->next == NULL) {
				//case for last node in the list is reached
				if((p->end_time)<=(_start)) {
					//printf("DEBUG!: child %d: check_overlap(): last position, overlap\n", member->id);
					return 0;			//debug
				} else if((p->end_time)>(_start)) {
					//printf("DEBUG!: child %d: check_overlap(): last position, no overlap\n", member->id);
					return 1;
				}
				break;

			} else  if((_start)>=(p->end_time)) {
				//case for last node hasn't reached
				//printf("DEBUG!: child %d: check_overlap(): checking\n", member->id);
				if((_start)>=(r->end_time)) {				//check if the slot is the right slot
					//if not reaching the right slot, continue
					//printf("DEBUG!: child %d: check_overlap(): check if continue searching\n", member->id);
					continue;
				}
				//right slot found
				//printf("DEBUG!: child %d: check_overlap(): %s\n", member->id, ctime(&(p->end_time)));		//debug!
				//printf("DEBUG!: child %d: check_overlap(): %s\n", member->id, ctime(&(r->end_time)));
				break;
			}
		}
	}

	//examine width of slot
	time_t starting = _start;
	time_t ending = r->start_time;

	//debug!
	//printf("DEBUG!: child %d: check_overlap(): slot start %s\n", member->id, ctime(&starting));
	//printf("DEBUG!: child %d: check_overlap(): slot end %s\n", member->id, ctime(&ending));

	//compute availability of slot in the list
	double slot = difftime(ending, starting);
	//debug!
	//printf("DEBUG!: check_overlap(): time slot allowed is %f\n", slot/60.0/60.0);

	//determine if slot is wide enough
	if(slot>=duration)
		//slot is enough to add a event
		return 0;
	else
		//slot is not wide enough
		return 1;
}

//function to add event to link list, member=struct User, command=instruction from user; flag: 1 = add to appointment, 0  to reject
void eventAdd(user *member, char *command, int flag) {

	//create new event and allot space
	event *q = (event*)malloc(sizeof(event));
	char *token;
	char *tok;
	int i = 0, j = 0;
	int id;
	char caller[20];

	//set flag if one of the participant not available
	/*for(i=0; i<10; i++) {
		if(availability[i]==0) {
			flag = 0;
		}
	}*/

	//dublicate command to keep safe
	char command_cc[80];
	strcpy(command_cc, command);

	//determine class of event
	token = strtok(command_cc, " :-.");
	if(strcmp(token, "addClass")==0)
		priority = 3;
	else if(strcmp(token, "addMeeting")==0)
		priority = 2;
	else if(strcmp(token, "addGathering")==0)
		priority = 1;

	//determine caller's name and store to char caller[]
	token = strtok(NULL, " .:-");
	strcpy(caller, token);
	caller[0] = toupper(caller[0]);
	//printf("DEBUG!: child %d: eventAdd(): caller's name is %s\n", member->id, caller);

	//extract time and priority level from command
	parse_time(command, &(q->start_time), &(q->end_time));
	q->priority = priority;
	i = 0;

	//read user name if evnt involves multiple users
	if(flag==1&&(priority==2 || priority==1)) {
		//case for event is added
		//store caller's name as first participant
		strcpy(q->usr[i], caller);
		i++;
		//store subsequent participants
		while((token=strtok(NULL, " :-."))!=NULL) {
			strcpy(q->usr[i], token);
			q->usr[i][0] = toupper(q->usr[i][0]);
			i++;
		}
		//store no. of participant
		q->parti_no = i;
	} else {
		//case for event is rejected
		//add name of the absentee
		strcpy(q->usr[0], absentee);
		//no. of name stored in q->usr[] is 1 for this case
		q->parti_no = 1;
	}


	event *p, *r;

	//link up the lis
	if(flag==0) {
		//case for rejected event, add to rejection list
		//printf("DEBUG!: child %d: eventAdd(): adding to reject list\n", member->id, member->name);
		if((member->rej_start)==NULL) {
			member->rej_start = q;
			member->rej_end = member->rej_start;
		} else {
			member->rej_end->next = q;
			member->rej_end = q;
		}
		member->rejection_count++;							//add rejected event
	} else if(flag==1) {
		//case for accepted event, add to appointment list
		p = member->start;

		if(p==NULL ||((q->end_time)<=(p->start_time))) {
			//case where list is empty or adding to the top of the list
			q->next = member->start;
			member->start = q;
			member->event_count++;
			//printf("DEBUG!: child %d: empty list or add to 1st position\n", member->id);				//debug!
			return;
		}
		//not to be added to first position, initiate searching for the right slot
		for(p=member->start, r=p->next; p!=NULL; p=p->next, r=p->next) {		//find position, p = one item before the positon
			if(p->next == NULL) {
				//case where event is added to last of the list
				//printf("DEBUG!: child %d: add to last\n", member->id);					//debug
				break;
			}

			if((q->start_time)>=(p->end_time)) {
				//compare and find slot
				if((q->start_time)>=(r->end_time)) {				//check if the slot is the right slot
					continue;
				}
				//printf("DEBUG!: child %d: finding position...\n", member->id);				//debug!
				//printf("DEBUG!: child %d: p %d, r %d\n", member->id, p->end_time, r->end_time);		//debug!
				//printf("DEBUG!: child %d: add to middle\n", member->id);
				break;
			}
		}
		//case for adding to 2nd or later position
		q->next = p->next;
		p->next = q;
		member->event_count++;
	}
}

//report generation function, usage: usr=struct User, output=output file name, flag: 1=appointment; 2=rejection; 3=app+rej
void report(user *usr, char output[10], int flag) {

	int i = 0;
	event *p;
	struct tm *time_info;
	char string[20];
	FILE *file;

	//open or create file, name specified by user
	file = fopen(output, "w");

	if(flag==1 || flag==3) {

		fprintf(file, "***Appointment Schedule***\n\n");

		fprintf(file, "%s, you have %d appointments\n\n", usr->name, usr->event_count);

		fprintf(file, "Date\t\tstart\tend\ttype\t\tpeople\t\t\n");
		fprintf(file, "==========================================================================================\n");

		for(p=usr->start; p!=NULL; p=p->next) {
			//printing time data
			time_info = localtime(&(p->start_time));
			strftime(string, 15, "%Y-%m-%d", time_info);
			fprintf(file, "%s\t", string);
			strftime(string, 15, "%H:%M", time_info);
			fprintf(file, "%s\t", string);
			time_info = localtime(&(p->end_time));
			strftime(string, 15, "%H:%M", time_info);
			fprintf(file, "%s\t", string);

			//print event type
			if((p->priority)==3)
				fprintf(file, "Class\t\t");
			else if((p->priority)==2)
				fprintf(file, "Meeting\t\t");
			else if((p->priority)==1)
				fprintf(file, "Gathering\t");

			//print participant
			for(i=0; i<(p->parti_no); i++) {
				fprintf(file, "%s ", p->usr[i]);
			}

			//return
			fprintf(file, "\n");
		}
		fprintf(file, "\n--------------------------------------------the end-------------------------------------------\n\n");

		printf("->[appointment list generated]\n");
	}

	if(flag==2 || flag==3) {

		fprintf(file, "***Rejected List***\n\n");

		fprintf(file, "%s, you have %d rejected appointment\n\n", usr->name, usr->rejection_count);

		fprintf(file, "Date\t\tstart\tend\ttype\t\treasons\t\t\n");
		fprintf(file, "==========================================================================================\n");

		for(p=usr->rej_start; p!=NULL; p=p->next) {
			//printing time data
			time_info = localtime(&(p->start_time));
			strftime(string, 15, "%Y-%m-%d", time_info);
			fprintf(file, "%s\t", string);
			strftime(string, 15, "%H:%M", time_info);
			fprintf(file, "%s\t", string);
			time_info = localtime(&(p->end_time));
			strftime(string, 15, "%H:%M", time_info);
			fprintf(file, "%s\t", string);

			//print event type
			if((p->priority)==3)
				fprintf(file, "Class\t\t");
			else if((p->priority)==2)
				fprintf(file, "Meeting\t\t");
			else if((p->priority)==1)
				fprintf(file, "Gathering\t");

			//print participant
			for(i=0; i<(p->parti_no); i++) {
				fprintf(file, "%s ", p->usr[i]);
			}
			fprintf(file, " not available");

			//return
			fprintf(file, "\n");
		}

		fprintf(file, "\n--------------------------------------------the end-------------------------------------------\n\n");

		printf("->[rejection list generated]\n");
	}

	fclose(file);

}

void parent(user *usr) {

	char string[80];
	char command[80];
	int caller, participant_id[9], feedback, absentee_id;
	char *token;
	int option = 10, t, i, j, flag;
	int availability[10];
	event *q = (event*)malloc(sizeof(event));
	printf("~Welcome to AOS~\n");
	printf("please enter appointment:\n");
	/*for(i=0; i<10; i++) {
		close(P2C[i][0]);
		close(C2P[i][1]);
	}*/
	while(1) {
		//read command
		strcpy(command, " ");
		strcpy(string, " ");
		fgets(command, 79, stdin);
		command[strlen(command)-1] = 0;
		//printf("DEBUG!: parent: length of command is %d\n", strlen(command));

		//analysis instruction
		strcpy(string, command);
		token = strtok(string, " -:.");
		//printf("DEBUG!: parent: command: %s\n", command);

		if(strcmp(token, "addClass")==0)
			option = 0;
		else if(strcmp(token, "addMeeting")==0)
			option = 1;
		else if(strcmp(token, "addGathering")==0)
			option = 1;
		else if(strcmp(token, "printSchd")==0)
			option = 2;
		else if(strcmp(token, "endProgram")==0)
			option = 3;
		//printf("DEBUG!: parent: option is %d\n", option);

		//engage in operation with child
		switch(option) {
			case 0:
				//add  class
				//printf("DEBUG!: parent: case 0\n");

				//get caller ID
				token = strtok(NULL, " -:.");
				token[0] = toupper(token[0]);
				//printf("DEBUG!: parent: name of caller is %s\n", token);

				//get user id
				for(i=0; i<child_no; i++) {
					if(strcmp(token, usr[i].name)==0)
						break;
				}

				caller = i;
				//printf("DEBUG!: parent: caller's id is %d\n", caller);
				int writer = 0;

				//creating node
				t = 0;

				//sending command to child
				close(P2C[caller][0]);
				write(P2C[caller][1], &t, sizeof(int));
				write(P2C[caller][1], &command, strlen(command));
				//close(P2C[caller][1]);
				//printf("DEBUG!: parent: instruction %d sent to child\n", t);

				t = 10;
				//reading feedback from child
				close(C2P[caller][1]);
				read(C2P[caller][0], &t, sizeof(int));
				//close(C2P[caller][0]);

				//give instruction to child according to feedback
				//printf("DEBUG!: parent: read from child, t = %d\n", t);

				if(t==0) {
					t = 1;
					close(P2C[caller][0]);
					write(P2C[caller][1], &t, sizeof(int));
					write(P2C[caller][1], &command, 80);
					//close(P2C[caller][1]);
					printf("->[accepted]\n");
				} else {
					t = 2;
					close(P2C[caller][0]);
					write(P2C[caller][1], &t, sizeof(int));
					write(P2C[caller][1], &command, 80);
					//close(P2C[caller][1]);
					printf("->[rejected] - %s is unavailable\n", usr[caller].name);
				}
				break;
			case 1:
				//add meeting or gathering
				//printf("DEBUG!: parent: case 1\n");

				//get caller ID
				token = strtok(NULL, " -:.");
				token[0] = toupper(token[0]);
				//printf("DEBUG!: parent: name of caller is %s\n", token);

				//get user id
				for(i=0; i<child_no; i++) {
					if(strcmp(token, usr[i].name)==0)
						break;
				}
				//DEBUG!
				caller = i;
				////printf("DEBUG!: parent: caller's id is %d\n", caller);
				writer = 0;

				//get participants
				for(i=0; i<8; i++) {
					token = strtok(NULL, " :.-");
				}
				//printf("DEBUG!: parent: ready to read names\n");
				j = 0;
				
				while(token!=NULL) {
					token[0] = toupper(token[0]);
					for(i=0; i<child_no; i++) {
						if(strcmp(token, usr[i].name)==0) {
							participant_id[j] = i;
							//printf("DEBUG!: parent: participant %d is %s, id %d\n", j, usr[i].name, participant_id[j]);
							j++;
						}
					}
					token = strtok(NULL, " :-.");
				}
				participant_id[j] = caller;
				//printf("DEBUG!: parent: participant %d is %s, id %d\n", j, usr[caller].name, caller);

				//creating node
				t = 0;
				feedback = 0;

				//sending command to children
				for(i=0; i<j+1; i++) {	//need to be fixed!
					close(P2C[participant_id[i]][0]);
					write(P2C[participant_id[i]][1], &t, sizeof(int));
					write(P2C[participant_id[i]][1], &command, strlen(command));
					
					close(C2P[participant_id[i]][1]);
					read(C2P[participant_id[i]][0], &feedback, sizeof(int));
					//printf("DEBUG!: parent: child sent feedback %d\n", feedback);

					if(feedback==1) {
						absentee_id = participant_id[i];
						break;
					}
				}

				if(feedback==1) {
					//printf("DEBUG!: parent: child %d unavailable!\n", participant_id[i]);
					t = 3;
					close(P2C[caller][0]);
					write(P2C[caller][1], &t, sizeof(int));
					//printf("DEBUG!: parent: instruction %d sent\n", t);

					write(P2C[caller][1], &participant_id[i], sizeof(int));
					write(P2C[caller][1], &command, 80);

					write(P2C[caller][1], &(usr[absentee_id].name), sizeof(usr[absentee_id].name));
					printf("->[rejected] - %s is unavailable\n", usr[absentee_id].name);

				} else if(feedback==0) {
					for(i=0; i<j;i++) {
						t = 1;
						close(P2C[caller][0]);
						write(P2C[caller][1], &t, sizeof(int));
						write(P2C[caller][1], &command, 80);

						for(i=0; i<j; i++) {
							close(P2C[participant_id[i]][0]);
							write(P2C[participant_id[i]][1], &t, sizeof(int));
							write(P2C[participant_id[i]][1], &command, 80);
							//printf("DEBUG!: parent: instruction %d sent to child %d\n", t, participant_id[i]);
						}
					}
					printf("->[accepted]\n");

				}

				break;

			case 2:
				//print report
				//printf("DEBUG!: parent: case 2\n");
				token = strtok(NULL, " .-:");
				token[0] = toupper(token[0]);
				for(i=0; i<child_no; i++) {
					if((strcmp(token, usr[i].name))==0)
						break;
				}
				//printf("DEBUG!: parent: caller is %s, id %d\n", token, i);
				caller = i;

				t = 4;
				close(P2C[caller][0]);
				write(P2C[caller][1], &t, sizeof(t)); 
				write(P2C[caller][1], &command, sizeof(command));
				//printf("DEBUG!: parent: instruction %d sent to child %d\n", t, caller);

				break;

			case 3:
				t = 5;
				for(i=0; i<child_no; i++) {
					//sent terminating message
					close(P2C[i][0]);
					write(P2C[i][1], &t, sizeof(t));
				}
				/*
				while(1) {
					int status;
					pid_t done = wait( &status );
					if ( done == -1 ) {
						if ( errno == ECHILD ) break; // no more child processes
					} else {
						if ( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 ) {
							printf( "PID %d failed\n", done );
							exit(1);
						}
					}
				}*/
				wait(NULL);
				printf("->Bye!\n");
				exit(0);
				break;

		}
	}
}

void child(user *usr, int id) {

	char string[80];
	int cmd, i, n, t=999;
	int *this_P2C = P2C[id];
	int *this_C2P = C2P[id];
	int temp, flag;
	char *token;
	//close pipes of other child
	for(i=0; i<child_no; i++) {
		close(P2C[i][1]);
		close(C2P[i][0]);
		if(i!=id) {
			close(P2C[i][0]);
			close(C2P[i][1]);
		}
	}

	//printf("DEBUG!: child: my ID is %d\n", id);
	while(1) {
		//check if parent's alive
		if(getppid()==1) {
			printf("error!: child %d: parent process terminated, exiting...\n", usr->id);
			exit(1);
		}

		strcpy(string, " ");
		cmd = 0;
		while((read(this_P2C[0], &cmd, sizeof(int)))>0) {
		
			close(this_P2C[1]);
			//printf("DEBUG!: child %d: n = %d\n", usr->id, n);
			//printf("DEBUG!: child %d: received instruction: %d\n", usr->id, cmd);
			switch(cmd) {
				case 0:
					//check time
					read(this_P2C[0], string, sizeof(string));				//send command
					//printf("DEBUG!: child %d: read command: %s\n", usr->id, string);

					//close(this_P2C[0]);
					event *p = (event*)malloc(sizeof(event));

					//parse time from command
					parse_time(string, &(p->start_time), &(p->end_time));			//get time
					//printf("DEBUG!: child %d: start time: %d; end time: %d\n", usr->id, p->start_time, p->end_time);
					//printf("DEBUG!: child %d: start checking time slot...\n", usr->id);

					//check overlap with child
					t = check_overlap(usr, p->start_time, p->end_time);
					//printf("DEBUG!: child %d: overlap = %d\n", usr->id, t);
					close(this_C2P[0]);
					write(this_C2P[1], &t, sizeof(int));
					//close(this_C2P[1]);
					//printf("DEBUG!: child %d: operation completed...\n", usr->id);
					break;
				case 1:
					//add event
					close(this_P2C[1]);
					read(this_P2C[0], string, sizeof(string));
					//close(this_P2C[0]);
					eventAdd(usr, string, 1);
					//printf("DEBUG!: child %d: event start %d end %d priority %d\n", usr->id, usr->start->start_time, usr->start->end_time, usr->start->priority);
					break;
				case 2:
					//reject class
					close(this_P2C[1]);
					read(this_P2C[0], string, sizeof(string));
					//close(this_P2C[0]);
					eventAdd(usr, string, 0);
					break;
				case 3:
					//reject meeting and gathering
					close(this_P2C[1]);
					read(this_P2C[0], &temp, sizeof(int));
					read(this_P2C[0], string, sizeof(string));
					read(this_P2C[0], absentee, sizeof(absentee));

					eventAdd(usr, string, 0);
					
					break;

				case 4:
					//print report
					close(this_P2C[1]);
					read(this_P2C[0], string, sizeof(string));
					//printf("DEBUG!: child %d: received command %s\n", usr->id, string);
					token = strtok(string, " :.-");
					token = strtok(NULL, " :.-");
					token = strtok(NULL, " :.-");
					if(strcmp(token, "t")==0)
						flag = 1;
					else if(strcmp(token, "r")==0)
						flag = 2;
					else if(strcmp(token, "f")==0)
						flag = 3;
					else {
						printf("error!: invalid option\n");
						break;
					}
					//printf("DEBUG!: child %d: flag is %d\n", usr->id, flag);

					token = strtok(NULL, " :-");

					report(usr, token, flag);

					break;

				case 5:
					//end program
					//printf("DEBUG!: child %d: ending...\n", usr->id);
					exit(0);
					break;
				default:
					return;
			}
		}

		//close(this_P2C[1]);
		//close(this_C2P[0]);
		//wait(NULL);
	}
	////printf("DEBUG!: child %d: exit loop, ending...\n", usr->id);

}

int main(int argc, char **argv) {

	user *usr = (user*)malloc((argc-1)*sizeof(user));
	int i = 0;
	child_no = argc - 1;
	if(child_no<3 || child_no>10) {
		printf("error: number of user incorrect!\n");
		return -1;
	}
	parent_pid = getpid();
	//printf("DEBUG!: struct created\n");

	//initializing all users
	for(i=0; i<(argc-1); i++) {
		strcpy(usr[i].name, argv[i+1]);
		usr[i].name[0] = toupper(usr[i].name[0]);
		usr[i].id = i;
		usr[i].start = NULL;
		usr[i].end = NULL;
		usr[i].rej_start = NULL;
		usr[i].rej_end = NULL;
		usr[i].rejection_count = 0;
		usr[i].event_count = 0;
		//printf("DEBUG!: main(): usr id %d, name %s\n", usr[i].id, usr[i].name);
	}
	//printf("DEBUG!: names copied\n");

	//forking child processes
	for(i=0; i<(argc-1); i++) {
		pipe(P2C[i]);
		pipe(C2P[i]);
		//printf("DEBUG!: main(): forking %d child\n", i);
		usr[i].process_id = fork();
		//determine 
		if(usr[i].process_id==0) {
			child(&usr[i], i);
			break;
		} else if(usr[i].process_id>0) {
		}
	}

	//parent process
	if(getpid()==parent_pid) {
		//printf("DEBUG!: main(): I am Parent\n");
		parent(usr);
	}

	return 0;
}
