/* gcc -L/usr/lib/x86_64-linux-gnu qb-cli.c -o qb-cli -lcurl -ljson-c -lncursesw */
/* valgrind --leak-check=full --track-origins=yes ./qb-cli */
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <json-c/json.h>
#include <inttypes.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <locale.h>

#define eos(s) ((s)+strlen(s))

char *_URL = "http://localhost:8079";     /* url to test site */
char *ref = "";		/* referer to test site, if not specified then ref = url */
char *ptitle = "qBittorrent CLI Monitor";
int torrent_size, categories_size;
int width,height,name_win_width,stat_win_width,win_height,name_win_x,stat_win_x,column_space;
int title = 1, menu = 3, rscr = 0, intv = 20, col = 8, choice = 0, n_col = 3, max_len = 0, main_selection = 1,bwidth = 1, cat_selection = 1;
char **name,***status,***info,**hash,**categories,**server,*sbuf;
char *sta_t[]={"DL(kb/s)","UL(kb/s)","Prog(%)","Ratio"};
char *inf_t[]={"State","Size","Balance","upload","ETA","Seed","Leeach","Category"};
char *srv_t[]={"Alltime DL","Alltime UL","DHT Nodes","Global Ratio"};
WINDOW *nwin, *swin, *sub;
time_t tnow;

/* holder for curl fetch */
struct curl_fetch_st {
	char *payload;
	size_t size;
};
struct tm *time_info;

struct curl_fetch_st curl_fetch;

int get_curl(void);
CURLcode curl_fetch_url(CURL *ch, const char *_URL, const char *ref, struct curl_fetch_st *fetch);
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
struct json_object * findobj(struct json_object *jobj, const char *key);
void parse_json(char *res);
char *c2fs(int64_t d);
void getscr(void);
static void print_name_win(int h);
static void print_stat_win(int choice);
int dlen(int d);
void freeAll(void);
void print_menu(char m);
int post_delete(int choice);
int post_resume(int choice);
int post_resumeAll();
int post_pause(int choice);
int post_pauseAll();
int post_category(int choice, char *cat);
int json_object_count(struct json_object *jobj);
char *c2time(int64_t d);
void print_setting_pop();
void print_action_pop();
void print_edit_pop();
void print_statistic_pop();
char *URLString(char* u, char* s);

int main() {
	int c, s_pop_flag = 0, a_pop_flag = 0, e_pop_flag = 0, interval_flag = 0, category_flag = 0, statistic_flag = 0, i_intv = 2;
	char ns[1];
	char dbuf[18];
	char cat[20];
	/* Set ref = URL if is empty */
	if(ref == NULL) {
		ref = _URL;
	}
	/* reset max_len */
	max_len = 0;
	/* Set Locale */
	setlocale(LC_ALL, "en_SG.UTF-8");
	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	getscr();
	nwin = newwin(win_height, name_win_width, title, name_win_x);
	swin = newwin(win_height, stat_win_width, title, stat_win_x);
	while(1) {
		time(&tnow);
		time_info=localtime( &tnow );
		strftime(dbuf,18,"%d %b %I:%M:%S%p", time_info);
		int g = get_curl();
		if(g == 0) {
			halfdelay(i_intv);	/* skip if no key detected */
			c = getch();	/* catch input from keyboard */
			switch(c) {
				case 'q':	/* q for quit */
					if(choice == 0) {
						endwin();
						exit(0);
					}
					break;
				case 'n':
						if(s_pop_flag == 1) {
							s_pop_flag = 0;
							interval_flag = 1;
						}
					break;
				case KEY_RESIZE:
					rscr = 1;
					break;
				case KEY_UP:
					if( torrent_size != 0) {
						if(main_selection == 1) {
							main_selection = torrent_size;
						}else{
							--main_selection;
						}
					}
					break;
				case KEY_DOWN:
					if( torrent_size != 0) {
						if(main_selection == torrent_size) {
							main_selection = 1;	/* start from 1 ~ */
						}else{
							++main_selection;
							}
					}
					break;
				case KEY_LEFT:
					if(category_flag == 1) {
						if(cat_selection == categories_size) {
							cat_selection = 1;
						}else{
							++cat_selection;
						}
					}
					break;
				case KEY_RIGHT:
					if(category_flag == 1) {
						if(cat_selection == 1) {
							cat_selection = categories_size;
						}else{
							--cat_selection;
						}
					}
					break;
				case 10:
					if( torrent_size != 0) {
						if(category_flag != 1) {
							/* close all pop memnu */
							s_pop_flag = 0;
							a_pop_flag = 0;
							e_pop_flag = 0;
							choice = main_selection;
						}else{
							post_category((choice - 1),categories[cat_selection - 1]);
							category_flag =0;
						}
					}
					break;
				case 'x':	/* for close info window */
					if(choice != 0) {
						if(e_pop_flag == 1) {
							e_pop_flag = 0;
						}
						choice = 0;
					}
					if(statistic_flag == 1) {
						statistic_flag = 0;
					}
					break;
				case 'r':
					if((choice == 0) && (a_pop_flag == 1)){
						post_resumeAll();
						a_pop_flag = 0;
					}
					if((choice != 0) && (e_pop_flag == 1)) {
						post_resume(choice - 1);
						e_pop_flag = 0;
					}
					break;
				case 'd':
					if((choice != 0) && (e_pop_flag == 1)) {
						post_delete(choice - 1);
						e_pop_flag = 0;
						choice = 0;
					}
				case 'p':
					if((choice == 0) && (a_pop_flag == 1)){
						post_pauseAll();
						a_pop_flag =0;
					}
					if((choice != 0) && (e_pop_flag == 1)) {
						post_pause(choice - 1);
						e_pop_flag =0;
					}
					break;
				case 'c':
					if((choice != 0) && (e_pop_flag == 1)){
						category_flag = 1;
						e_pop_flag = 0;
					}
					break;
				case 's':
					if((choice == 0) && (a_pop_flag == 0)){
						if(s_pop_flag == 0) {
							s_pop_flag = 1;
						}else{
							s_pop_flag = 0;
						}
					}
					break;
				case 'a':
					if((choice == 0) && (s_pop_flag == 0)){
						if(a_pop_flag == 0) {
							a_pop_flag = 1;
						}else{
							a_pop_flag = 0;
						}
					}
					break;
				case 'e':
					if(choice != 0) {
						if(e_pop_flag == 0) {
							e_pop_flag = 1;
						}else{
							e_pop_flag =0;
						}
					}
				case 'i':
					if(s_pop_flag == 1) {
						s_pop_flag = 0;
						statistic_flag = 1;
					}
					break;
				case 27:
					if(category_flag == 1) {
						category_flag = 0;
					}
					break;
				default:
					/* any other input will trigger here such as resize terminal */
					//printf("Character press: %3d Print as '%c' \n",c,c);
					break;
			}
			/* update for screen resize */
			if(rscr = 1) {
				getscr();
				delwin(nwin);
				delwin(swin);
				nwin = newwin(win_height, name_win_width, title, name_win_x);
				swin = newwin(win_height, stat_win_width, title, stat_win_x);
				rscr =0;
			}
			/* draw to our windows */
			wclear(nwin);
			wclear(swin);
			wclear(stdscr);
			attron(A_BOLD | A_UNDERLINE);
			mvprintw(0, 0,"%-s",ptitle);
			attroff(A_BOLD | A_UNDERLINE);
			mvprintw(0, strlen(ptitle),"%*s\n",((stat_win_x + stat_win_width) - strlen(ptitle)),dbuf);
			print_name_win(main_selection);
			print_stat_win(choice);
			/* refresh each window */
			wnoutrefresh(stdscr);
			wnoutrefresh(nwin);
			wnoutrefresh(swin);
			/* draw setting pop up */
			if(s_pop_flag == 1) {
				print_setting_pop();
				wnoutrefresh(sub);
			}
			if(a_pop_flag == 1) {
				print_action_pop();
				wnoutrefresh(sub);
			}
			if(e_pop_flag == 1) {
				print_edit_pop();
				wnoutrefresh(sub);
			}
			if(interval_flag == 1) {
				echo();
				print_menu('n');
				getstr(ns);
				if( atoi(ns) > 0 ) {intv = atoi(ns)*10;}
				noecho();
				interval_flag = 0;
			}
			if(category_flag == 1) {
				// echo();
				print_menu('c');
				// getstr(cat);
				// noecho();
				// if(strlen(cat) != 0) {post_category((choice - 1),cat);}
				//category_flag =0;
			}
			if(statistic_flag == 1) {
				print_menu('a');
				print_statistic_pop();
				wnoutrefresh(stdscr);
				wnoutrefresh(sub);
			}
			doupdate();
			freeAll();
		}
		if(i_intv != intv) {
			i_intv = intv;
		}
	}
	endwin();
	exit(0);
}
/* curl get */
int get_curl(void) {
	CURL *ch;                                               /* curl handle */
	CURLcode rcode;                                         /* curl result code */
	struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
	if ((ch = curl_easy_init()) == NULL)
	{      	fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");	/* log error */
        	return 1;	/* return error */
    }
	char *url_string = URLString(_URL, "/sync/maindata");
	rcode = curl_fetch_url(ch, url_string, ref, cf);	/* fetch page and capture return code */
	/* cleanup curl handle */
	curl_easy_cleanup(ch);
	free(url_string);
    if (rcode != CURLE_OK || cf->size < 1) {	/* check return code */
		fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s", url_string, curl_easy_strerror(rcode));	/* log error */
        return 2;	/* return error */
    }
	/* check payload */
    if (cf->payload != NULL) {
		parse_json(cf->payload);	/* print result */
		free(cf->payload);	/* free payload */
		return 0;
	}else{
        fprintf(stderr, "ERROR: Failed to populate payload");	/* error */
        free(cf->payload);	/* free payload */
        return 3;	/* return */
    }
}
/* curl post delete*/
int post_delete(int choice) {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/delete");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	post = (char*)malloc((strlen(hash[choice]) + 6) * sizeof(char));
	sprintf(post,"hashes=%s",hash[choice]);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post));
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	if(post) {
		free(post);
	}
	return 0;
}

/* curl post resume*/
int post_resume(int choice) {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/resume");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	post = (char*)malloc((strlen(hash[choice]) + 6) * sizeof(char));
	sprintf(post,"hash=%s",hash[choice]);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post));
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	if(post) {
		free(post);
	}
	return 0;
}
/* curl post resumeAll*/
int post_resumeAll() {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/resumeAll");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	return 0;
}
/* curl post pause*/
int post_pause(int choice) {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/pause");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	post = (char*)malloc((strlen(hash[choice]) + 6) * sizeof(char));
	sprintf(post,"hash=%s",hash[choice]);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post));
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	if(post) {
		free(post);
	}
	return 0;
}
/* curl post pauseAll*/
int post_pauseAll() {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/pauseAll");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	return 0;
}
/* curl post category*/
int post_category(int choice, char *cat) {
	CURL *curl;
	CURLcode pcurl;
	char *post;
	curl = curl_easy_init();
	char *url_string = URLString(_URL, "/command/setCategory");
	curl_easy_setopt(curl, CURLOPT_URL, url_string);
	post = (char*)malloc((strlen(hash[choice]) + ((int)strlen(cat)) + 18) * sizeof(char));
	sprintf(post,"hashes=%s&category=%s",hash[choice],cat);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post));
	curl_easy_setopt(curl, CURLOPT_REFERER, ref);
	pcurl = curl_easy_perform(curl);
	if(pcurl != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(pcurl));
	}
	curl_easy_cleanup(curl);
	free(url_string);
	if(post) {
		free(post);
	}
	return 0;
}
/* fetch and return url body via curl */
CURLcode curl_fetch_url(CURL *ch, const char *url, const char *ref, struct curl_fetch_st *fetch) {
	CURLcode rcode;                                              /* curl result code */
	fetch->payload = (char *) calloc(1, sizeof(fetch->payload)); /* init payload */
	/* check payload */
	if (fetch->payload == NULL) {
		/* log error */
		fprintf(stderr, "ERROR: Failed to allocate payload in curl_fetch_url");
		/* return error */
		return CURLE_FAILED_INIT;
	}
	fetch->size = 0; /* init size */
	curl_easy_setopt(ch, CURLOPT_URL, url); /* set url to fetch */
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback); /* set calback function */
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) fetch); /* pass fetch struct pointer */
	curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0"); /* set default user agent */
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 5); /* set timeout */
	curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1); /* enable location redirects */
	curl_easy_setopt(ch, CURLOPT_MAXREDIRS, 1); /* set maximum allowed redirects */
	curl_easy_setopt(ch, CURLOPT_REFERER, ref); /* set referer */
	rcode = curl_easy_perform(ch); /* fetch the url */
	return rcode;
}
/* call back for curl */
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;                                       /* calculate buffer size */
	struct curl_fetch_st *p = (struct curl_fetch_st *) userp;             /* cast pointer to fetch struct */
	p->payload = (char *) realloc(p->payload, p->size + realsize + 1);    /* expand buffer */
	if (p->payload == NULL) {	/* check buffer */
		/* this isn't good */
		fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
		/* free buffer */
		free(p->payload);
		/* return */
		return -1;
	}
	memcpy(&(p->payload[p->size]), contents, realsize); /* copy contents to buffer */
	p->size += realsize;                                /* set new buffer size */
	p->payload[p->size] = 0;                            /* ensure null termination */
	return realsize;                                    /* return size */
}
/* find json object */
struct json_object * findobj(struct json_object *jobj, const char *key) {
	struct json_object *element;
	json_object_object_get_ex(jobj, key, &element);
	return element;
}
/* parse json object */
void parse_json(char *res) {
	struct json_object *jdata,*data,*data2,*tmp;
	jdata = json_tokener_parse(res);
	/* Parse categories list */
	json_object_object_get_ex(jdata,"categories",&data);
	if(data != NULL) {
		categories = malloc(json_object_array_length(data) * sizeof(char*));
		for(int i = 0; i < json_object_array_length(data); ++i) {
			tmp = json_object_array_get_idx(data,i);
			categories[i] = malloc((strlen(json_object_get_string(tmp)) + 1) * sizeof(char));
			strcpy(categories[i],json_object_get_string(tmp));
			categories_size = i + 1;
		}
	}
	/* Parse server status */
	json_object_object_get_ex(jdata,"server_state",&data);
	if(data != NULL) {
		server = malloc((sizeof(srv_t)/sizeof(srv_t[0])) * sizeof(char*));
		server[0] = c2fs(json_object_get_int64(findobj(data,"alltime_dl")));
		server[1] = c2fs(json_object_get_int64(findobj(data,"alltime_ul")));
		server[2] = malloc(8 * sizeof(char));
		sprintf(server[2],"%d",(int)json_object_get_int(findobj(data,"dht_nodes")));
		server[3] = malloc(8 * sizeof(char));
		sprintf(server[3],"%.*f",2, (double)json_object_get_double(findobj(data,"global_ratio")));
	}
	/* Parse torrent */
	json_object_object_get_ex(jdata,"torrents",&data);
	if(data != NULL) {
		/* Get torrent size */
		torrent_size = json_object_count(data);
		hash = malloc(torrent_size * sizeof(char*));
		name = malloc(torrent_size * sizeof(char*));
		status = malloc(torrent_size * sizeof(char**));
		info = malloc(torrent_size * sizeof(char**));
		int j = 0;
		json_object_object_foreach(data, key2, val2) {
			hash[j] = malloc((strlen(key2) + 1) * sizeof(char));
			sprintf(hash[j],"%s",key2);
			json_object_object_get_ex(data,key2,&data2);
			name[j] = malloc((strlen(json_object_get_string(findobj(data2,"name"))) + 1) * sizeof(char));
			strcpy(name[j],json_object_get_string(findobj(data2,"name")));
			if(strlen(name[j]) > max_len) {	/* set maximum name len */
				max_len = strlen(name[j]);
			}
			status[j] = malloc((sizeof(sta_t)/sizeof(sta_t[0])) * sizeof(char*));
			status[j][0] = malloc(8 * sizeof(char)); /* 0 - dspd */
			sprintf(status[j][0],"%.*f",2,(double)json_object_get_int(findobj(data2,"dlspeed"))/1000);
			status[j][1] = malloc(8 * sizeof(char)); /* 1 - uspd */
			sprintf(status[j][1],"%.*f",2,(double)json_object_get_int(findobj(data2,"upspeed"))/1000);
			status[j][2] = malloc(8 * sizeof(char)); /* 2 - prgs */
			sprintf(status[j][2],"%.*f",2,(float)json_object_get_double(findobj(data2,"progress")) * 100);
			status[j][3] = malloc(8 * sizeof(char)); /* 3 - ratio */
			sprintf(status[j][3],"%.*f",3,(double)json_object_get_double(findobj(data2,"ratio")));
			/* info list - for multi dimensional array */
			info[j] = malloc((sizeof(inf_t)/sizeof(inf_t[0])) * sizeof(char*));
			info[j][0] = malloc((strlen(json_object_get_string(findobj(data2,"state"))) + 1) * sizeof(char));
			strcpy(info[j][0],json_object_get_string(findobj(data2,"state")));	/* 0 - state */
			info[j][1] = c2fs(json_object_get_int64(findobj(data2,"total_size")));	/* 1 - file size */
			info[j][2] = c2fs(json_object_get_int64(findobj(data2,"amount_left")));	/* 2 - balance size */
			info[j][3] = c2fs(json_object_get_int64(findobj(data2,"uploaded")));	/* 3 - total uploaded */
			info[j][4] = c2time(json_object_get_int64(findobj(data2,"eta"))); /* convert to time date -d @int */
			info[j][5] = malloc((dlen(json_object_get_int(findobj(data2,"num_seeds")))+dlen(json_object_get_int(findobj(tmp,"num_complete"))) + 3) * sizeof(char)); /* 4 - seed */
			sprintf(info[j][5],"%d(%d)",json_object_get_int(findobj(data2,"num_seeds")),json_object_get_int(findobj(tmp,"num_complete")));
			info[j][6] = malloc((dlen(json_object_get_int(findobj(data2,"num_leechs")))+dlen(json_object_get_int(findobj(tmp,"num_incomplete"))) + 3) * sizeof(char)); /* 4 - seed */ /* 5 - leech */
			sprintf(info[j][6],"%d(%d)",json_object_get_int(findobj(data2,"num_leechs")),json_object_get_int(findobj(tmp,"num_incomplete")));
			info[j][7] = malloc((strlen(json_object_get_string(findobj(data2,"category"))) + 1) * sizeof(char)); /* get category */
			strcpy(info[j][7],json_object_get_string(findobj(data2,"category")));
			j++;
		}
	}
	json_object_put(jdata);
}
/* free all malloc */
void freeAll(void) {
	if(categories_size != 0) {
		for(int i = 0; i < categories_size; ++i) {
			free(categories[i]);
		}
		free(categories);
	}
	if((sizeof(srv_t)/sizeof(srv_t[0])) != 0) {
		for(int i = 0; i < (sizeof(srv_t)/sizeof(srv_t[0])); ++i) {
			free(server[i]);
		}
		free(server);
	}
	if(torrent_size != 0) {
		for(int j = 0; j < torrent_size; ++j) {
			free(name[j]);
			for(int s = 0; s < (sizeof(sta_t)/sizeof(sta_t[0])); ++s) {
				free(status[j][s]);
			}
			for(int d = 0; d < (sizeof(inf_t)/sizeof(inf_t[0])); ++d) {
				free(info[j][d]);
			}
			free(info[j]);
			free(hash[j]);
			free(status[j]);
		}
		free(name);
		free(status);
		free(info);
		free(hash);
	}
	if(sbuf) {
		free(sbuf);
	}
}
/* convert integer to human read file size */
char *c2fs(int64_t d) {
	int count=0;
	double gb=1000000000;
	double mb=1000000;
	char *buff = malloc((col+1) * sizeof(char));
	double tmp = (double)d;
	if(d == 0) {
		sprintf(buff,"%"PRId64,d);
	}else{
		while(d != 0) {
			d /= 10;
			++count;
		}
		if(count > 9) {
			/* GB */
			sprintf(buff,"%.*fGB",2,tmp/gb);
		}
		if((count <= 9)&&(count > 6)) {
			/* MB */
			sprintf(buff,"%.*fMB",2,tmp/mb);
		}
		if((count <= 6) && (count > 0)) {
			/* KB */
			sprintf(buff,"%.*fKB",2,tmp/1000);
		}
	}
	return buff;
}
/* get screen size & cal printing variable */
void getscr(void) {
	getmaxyx(stdscr, height, width); /* <= slower & unreliable */
	if(max_len == 0) {
		max_len = 18;
	}
	if(width < (max_len + (3 * col) + bwidth)) {
		n_col = 3;
		stat_win_x = width - (n_col * col) - bwidth;
		column_space = col;
	}else{
		stat_win_x = max_len + 1;
		if(width >= max_len + (4 * col) + bwidth) {
			n_col = 4;	/* set n_col=4; */
		}else{
			n_col = 3;	/* else used 3 col */
		}
		for(int i = 0; i <= 7; i++) {
			if((max_len + (n_col * (col + i)) + bwidth) <= width) {
				column_space = col + i;
			}
		}
	}
	win_height = height - title - menu;
	name_win_width = max_len + 1;
	stat_win_width = (column_space * n_col) + bwidth;
	name_win_x = 0;
}
/* print name window */
static void print_name_win(int h) {
	mvwprintw(nwin, 0, 0, "%-*s", name_win_width, "Name");
	if(torrent_size != 0) {
		for(int i = 0; i < torrent_size; ++i) {
			if(h == (i + 1)){
				wattron(nwin, A_REVERSE);
				if((width - stat_win_width - bwidth) < strlen(name[i])) {
					mvwprintw(nwin,(i + 1), 0, "%-.*s..", (width - stat_win_width - 4), name[i]);
				}else{
					mvwprintw(nwin,(i + 1), 0, "%-s",name[i]);
				}
				wattroff(nwin,A_REVERSE);
			}else{
				if((width - stat_win_width - bwidth) < strlen(name[i])) {
					mvwprintw(nwin, (i + 1), 0, "%-.*s..", (width - stat_win_width - 4), name[i]);
				}else{
					mvwprintw(nwin, (i + 1), 0, "%-s", name[i]);
				}
			}
		}
	}
}
/* print status window */
static void print_stat_win(int choice) {
	if(choice == 0) {
		/* print status window */
		sbuf = malloc(column_space  * sizeof(sbuf));
		sprintf(sbuf,"%*s", column_space, sta_t[0]);
		for(int s = 1; s < n_col; ++s) {
			int t = (int)strlen(sbuf);
			sbuf = (char *)realloc(sbuf, (t + column_space) * sizeof(sbuf));
			sprintf(eos(sbuf), "%*s", column_space, sta_t[s]);
		}
		sprintf(eos(sbuf), "\n");
		if( torrent_size != 0) {
			for(int j = 0; j < torrent_size; ++j) {
				for(int i = 0;i < n_col; ++i) {
					int t = (int)strlen(sbuf);
					sbuf = (char *)realloc(sbuf, (t + column_space) * sizeof(sbuf));
					sprintf(eos(sbuf), "%*s", column_space, status[j][i]);
				}
				sprintf(eos(sbuf), "\n");
			}
		}
		print_menu('s');
	}else{
		/* print info window */
		sbuf = malloc( 12 * sizeof(sbuf));
		sprintf(sbuf, "%s\n", "Info Window");
		for(int s = 0; s < (sizeof(sta_t)/sizeof(sta_t[0])); ++s) {
			int t = (int)strlen(sbuf);
			sbuf = (char *)realloc(sbuf, (t + column_space + (int)strlen(status[choice-1][s]) + 4) * sizeof(sbuf));
			sprintf(eos(sbuf), "%-*s = %s\n", column_space, sta_t[s], status[choice-1][s]);
		}
		for(int s = 0; s < (sizeof(inf_t)/sizeof(inf_t[0])); ++s) {
			int t = (int)strlen(sbuf);
			sbuf = (char *)realloc(sbuf, (t + column_space + (int)strlen(info[choice-1][s]) + 4) * sizeof(sbuf));
			sprintf(eos(sbuf), "%-*s = %s\n", column_space, inf_t[s], info[choice-1][s]);
		}
		print_menu('i');
	}
	mvwprintw(swin,0, 0, "%s", sbuf);
}
/* return number of digit */
int dlen(int d) {
	int c = 0;
	if( d == 0) {
		c =1;
	}else{
		while(d != 0) {
			d /= 10;
			++c;
		}
	}
	return c;
}
/* print menu list */
void print_menu(char m) {
	char main_sym[]={'s','a','q'};
	char *main_desc[]={"Setting","Action","Quit"};
	char info_sym[]={'e','x'};
	char *info_desc[]={"Edit","Close"};
	char statistic_sym[]={'x'};
	char *statistic_desc[]={"Close"};
	char *categories_sym[]={"\xE2\xA5\xAE", "\xE2\xAE\xA0"};
	char *categories_desc[]={"Select", "Enter"};
	int y = height - 1;
	/* Print main menu */
	if(m == 's') {
		move((y - 2),0);
		wclrtobot(stdscr);
		attron(A_UNDERLINE);
		mvprintw((y - 1), 0, "%*s\n", (name_win_width + stat_win_width) - 1, " ");
		attroff(A_UNDERLINE);
		for(int i = 0; i < (sizeof(main_sym)/sizeof(main_sym[0])); ++i) {
			attron(A_REVERSE);
			mvaddch(y, (i * 15), main_sym[i]);
			attroff(A_REVERSE);
			mvaddstr(y, ((i * 15) + 2), main_desc[i]);
		}
	}
	/* Print info menu */
	if(m == 'i') {
		move((y - 2), 0);
		wclrtobot(stdscr);
		attron(A_UNDERLINE);
		mvprintw((y - 1), 0, "%*s\n", (name_win_width + stat_win_width) - 1, " ");
		attroff(A_UNDERLINE);
		for(int i = 0; i < (sizeof(info_sym)/sizeof(info_sym[0])); ++i) {
			attron(A_REVERSE);
			mvaddch(y, (i * 15), info_sym[i]);
			attroff(A_REVERSE);
			mvaddstr(y, ((i * 15) + 2), info_desc[i]);
		}
	}
	/* Print Statistic menu */
	if(m == 'a') {
		move((y - 2), 0);
		wclrtobot(stdscr);
		attron(A_UNDERLINE);
		mvprintw((y - 1), 0, "%*s\n", (name_win_width + stat_win_width) - 1, " ");
		attroff(A_UNDERLINE);
		for(int i = 0; i < (sizeof(statistic_sym)/sizeof(statistic_sym[0])); ++i) {
			attron(A_REVERSE);
			mvaddch(y, (i * 15), statistic_sym[i]);
			attroff(A_REVERSE);
			mvaddstr(y, ((i * 15) + 2), statistic_desc[i]);
		}
	}
	/* Print Interval menu */
	if(m == 'n') {
		move((y - 2), 0);
		wclrtobot(stdscr);
		attron(A_UNDERLINE);
		mvprintw((y-2), 0, "%*s\n", (name_win_width + stat_win_width) - 1, " ");
		attroff(A_UNDERLINE);
		for(int i = 0; i < (sizeof(main_sym)/sizeof(main_sym[0])); ++i) {
			attron(A_REVERSE);
			mvaddch(y, (i * 15), main_sym[i]);
			attroff(A_REVERSE);
			mvaddstr(y, ((i * 15) + 2), main_desc[i]);
		}
		mvprintw((y - 1), 0, "Default[%d],Enter[1-9]: ", intv/10);
	}
	/* Print Category menu */
	if(m == 'c') {
		move((y - 2), 0);
		wclrtobot(stdscr);
		attron(A_UNDERLINE);
		mvprintw((y - 2), 0, "%*s\n", (name_win_width + stat_win_width) - 1, " ");
		attroff(A_UNDERLINE);
		for(int i = 0; i < (sizeof(categories_sym)/sizeof(categories_sym[0])); ++i) {
			//attron(A_REVERSE);
			mvaddstr(y, (i * 15), categories_sym[i]);
			//attroff(A_REVERSE);
			mvaddstr(y, ((i * 15) + 2), categories_desc[i]);
		}
		mvprintw((y - 1), 0, "Category: %s", categories[cat_selection - 1]);
	}
}
/* Print setting pop */
void print_setting_pop() {
	char pop_sym[]={'n','i'};
	char *pop_desc[]={"Interval","Statistics"};
	int s = (sizeof(pop_sym)/sizeof(pop_sym[0]));
	int y = height - 2 - (s + 1);
	sub = newwin((s + 1), 20, y, 0);
	for(int i = 0; i < (sizeof(pop_sym)/sizeof(pop_sym[0])); ++i) {
		wattron(sub, A_REVERSE);
		mvwaddch(sub, i, 0, pop_sym[i]);
		wattroff(sub, A_REVERSE);
		mvwaddstr(sub, i, 2, pop_desc[i]);
	}
	mvwaddstr(sub, s, 0, "\xE2\x96\xBC\n");
}
/* Print action pop */
void print_action_pop() {
	char pop_sym[]={'p','r'};
	char *pop_desc[]={"Pause All","Resume All"};
	int s = (sizeof(pop_sym)/sizeof(pop_sym[0]));
	int y = height - 2 - (s + 1);
	sub = newwin((s + 1), 20, y, 15);
	for(int i = 0; i < (sizeof(pop_sym)/sizeof(pop_sym[0])); ++i) {
		wattron(sub, A_REVERSE);
		mvwaddch(sub, i, 0, pop_sym[i]);
		wattroff(sub, A_REVERSE);
		mvwaddstr(sub, i, 2, pop_desc[i]);
	}
	mvwaddstr(sub, s, 0, "\xE2\x96\xBC\n");
}
/* Print edit pop */
void print_edit_pop() {
	char pop_sym[]={'p','r','d','c','f'};
	char *pop_desc[]={"Pause","Resume","Delete","Category","File List"};
	int s = (sizeof(pop_sym)/sizeof(pop_sym[0]));
	int y = height - 2 - (s + 1);
	sub = newwin((s + 1), 20, y, 0);
	for(int i = 0; i < (sizeof(pop_sym)/sizeof(pop_sym[0])); ++i) {
		wattron(sub, A_REVERSE);
		mvwaddch(sub, i, 0, pop_sym[i]);
		wattroff(sub, A_REVERSE);
		mvwaddstr(sub, i, 2, pop_desc[i]);
	}
	mvwaddstr(sub, s, 0, "\xE2\x96\xBC\n");
}
/* Print statistic pop */
void print_statistic_pop() {
	int sub_width = 30, sub_height = 10;
	int sub_center_x = ((name_win_width + stat_win_width)/2) - (sub_width/2);
	int sub_center_y = (height/2)-(sub_height/2);
	sub = newwin(sub_height, sub_width, sub_center_y, sub_center_x);
	wborder(sub,ACS_VLINE,ACS_VLINE,ACS_HLINE,ACS_HLINE,ACS_ULCORNER,ACS_URCORNER,ACS_LLCORNER,ACS_LRCORNER);
	wattron(sub, A_UNDERLINE);
	mvwprintw(sub, 1, 1, "Statistic");
	wattroff(sub, A_UNDERLINE);
	for(int i = 0; i < (sizeof(srv_t)/sizeof(srv_t[0])); ++i) {
		mvwprintw(sub, (i * 2) + 2, 1, "%-*s = %*s", 12, srv_t[i], 13, server[i]);
	}
}
/* convert to time */
char *c2time(int64_t d) {
	int day, hr, min, sec;
	char *buff = malloc(30 * sizeof(char));
	sec = d%60;
	min = (d/60)%60;
	hr = ((d/60)/60)%24;
	day = ((d/60)/60)/24;
	if(day > 0) {
		sprintf(buff,"%dd %dh %dm", day, hr, min);
	}
	if((day == 0) && (hr > 0)) {
		sprintf(buff,"%dh %dm %ds", hr, min, sec);
	}
	if((day == 0) && (hr == 0) && (min > 0)) {
		sprintf(buff,"%dm %ds", min, sec);
	}
	if((day == 0) && (hr == 0) && (min == 0) && (sec > 0)) {
		sprintf(buff,"%ds", sec);
	}
	return buff;
}
/* Return json object count */
int json_object_count(struct json_object *jobj) {
	int count = 0;
	json_object_object_foreach(jobj, key, val) {
		count++;
	}
	return count;
}
/* insert path into url */
char *URLString(char* u, char* s) {
	char *buff = malloc((int)(strlen(u) + (int)strlen(s) + 1) * sizeof(char));
	strcpy(buff,u);
	strcat(buff,s);
	return buff;
}

