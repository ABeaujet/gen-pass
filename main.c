#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <yaml.h>
#include <openssl/sha.h>

#include "debug_tools.h"

#define MIN(x, y) ( (x < y) ? x : y)
#define MAX(x, y) ( (x > y) ? x : y)

/**
  * Usage :
  * ./gen-pass [-dUlnXx] [-L pass_size] sitename.com
  * 
  * TODO: Register domain name changes in db
  *       Or maybe just *replace* the line ?
  * TODO: Change the display of password types from numbers to nice-looking list of checked and unchecked char classes.
  */

// PASSWORD TYPE MASK VALUES
#define TYPE_UPPER		0x01	// U
#define TYPE_LOWER		0x02	// l
#define TYPE_NUMBER		0x04	// n
#define TYPE_SPECIAL	0x08	// s (U+n = X, l+n = x)
#define TYPE_ALL		(TYPE_UPPER | TYPE_LOWER | TYPE_SPECIAL | TYPE_NUMBER)

// GLOBALS
bool debug = false;

// TYPEDEFS
typedef struct {
	unsigned int type;
	unsigned int length;
} password_requirement_t;

// THE BEEF

void print_usage(){
	printf("Usage : ./gen-pass [-dUlnXxs] [-L pass_size] sitename.com\n");
	printf("Note : (http(s)://)(www.) will be automatically trimmed.\n");
}

void print_hash(const unsigned char* in, size_t len){
	const unsigned char* c = in;
	for(unsigned int i = 0;i<len;i++)
		printf("%02x ", *c++);
	putchar('\n');
}

char get_ascii_char(password_requirement_t pass_req, int index){
	static const char special[] = {'!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '@', '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~' };
	#define special_size 32

	static const char upper[] = {'A','B','C','D','E','F','G','U','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
	#define upper_size 26

	static const char lower[] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
	#define lower_size 26

	static const char num[] = {'0','1','2','3','4','5','6','7','8','9'};
	#define num_size 10

	static const char const * lists[] = { upper, lower, num, special, NULL};
	static const int const sizes[] = { upper_size, lower_size, num_size, special_size, 0};
	static const int list_count = 4;

	int modulo = 0;
	unsigned int pass_type2 = pass_req.type;
	for(int i =0;i<list_count;i++){
		if(pass_type2 & 0x01)
			modulo += sizes[i];
		pass_type2 >>= 1;
	}

	index %= modulo;

	for(int i =0;i<list_count;i++)
		if(pass_req.type & 1 << i)
			if(index < sizes[i])
				return lists[i][index];
			else
				index -= sizes[i];

	printf("What the fuck\n");
	return -128; // whatever, can't happen. I should have not said that.
}

void gen_pass(char* master_pass, char* site_name, char* res, password_requirement_t pass_req){
	char* c = site_name;
	if(res == NULL)
		res = calloc(pass_req.length + 1, 1);
	int out_i = 0;
	unsigned char sha_site[33] = {'\0'};
	unsigned char sha_master[33] = {'\0'};
	memcpy(sha_site, SHA256(site_name, strlen(site_name), NULL), 32);
	memcpy(sha_master, SHA256(master_pass, strlen(master_pass), NULL), 32);

	_dprint("Domain hash : ");
	if(debug) print_hash(sha_site, 32);
	_dprint("Master hash : ");
	if(debug) print_hash(sha_master, 32);

	for(int i = 0;i<32;i++){
		out_i = i%pass_req.length;
		res[out_i] = get_ascii_char(pass_req, (res[pass_req.length] + sha_site[i] + sha_master[i]));
	}
	res[pass_req.length] = '\0';
}

char* trim_url(char* in){
	// remove http, www
	const char * patterns[] = {"http://", "https://", "www.", NULL};
	char * out = in;
	for(int i = 0;patterns[i] != NULL;i++){
		const char* curr_pat = patterns[i];
		bool match = true;
		int j = 0;
		for(;match && curr_pat[j] && out[j];j++)
			if(curr_pat[j] != out[j])
				match = false;
		if(match){
			_dprintf("Trimmed out %s\n", patterns[i]);
			out += j;
		}
	}
	// remove terminating forward slash
	size_t out_len = strlen(out);
	if(out[out_len - 1] == '/')
		out[out_len - 1] = '\0';
	return out;
}

int register_website_requirements(const char* domain, password_requirement_t pass_req){
	const char config_file[] = "./websites.lst";
	FILE* config = fopen(config_file, "a");
	if(config == NULL)
		return EXIT_FAILURE;
	fprintf(config, "%s:%u:%u\n", domain, pass_req.type, pass_req.length);
	fclose(config);
	return EXIT_SUCCESS;
}

char* get_line_start(char* l){
	char* c = l;
	while(*c && isblank(*c))
		c++;
	return c;
}

int get_website_config_line(const char* domain, int* line_number, char* config_line){
	const char config_file[] = "./websites.lst";
	FILE* config = fopen(config_file, "r+");
	if(config == NULL)
		goto fail;

	char* current_line = NULL;
	int current_line_number = 0;
	size_t current_line_size = 0;
	while(getline(&current_line, &current_line_size, config) >= 0){

		current_line_number++;

		// if(strlen(current_line) < 3) continue; Not sure actually.........;

		if(isblank(current_line[0])) // check for blank padding
			current_line = get_line_start(current_line);

		if(current_line[0] == '#') // it's a comment line.
			continue;

		char* delim; // ':' delimiters positions
		char current_domain[100] = {'\0'};

		// find the end of the domain part in the line
		delim = strchr(current_line, ':');
		if(delim == NULL){
			fclose(config);
			goto fail;
		}

		// check this is the right domain (and so the right line)
		memcpy(current_domain, current_line, delim-current_line);
		if(strcmp(domain, current_domain) != 0)
			continue;

		*line_number = current_line_number;

		if(config_line != NULL)
			memcpy(config_line, current_line, current_line_size);

		fclose(config);
		return 0;

	}
	fclose(config);
	return 1;

fail:
	fprintf(stderr, "Missing or corrupted syntax in config file.\n");
	return -1;
}

int get_website_requirements(const char* domain, password_requirement_t *pass_req){

	char current_line[200];
	int line_number = 0; // dummy
	int  err = 0;
	
	if( (err = get_website_config_line(domain, &line_number, current_line)) == 1)
		return 1;
	else if(err < 0)
		return -1;

	char* delim; // ':' delimiters positions
	password_requirement_t current_requirement;

	// find the end of the domain part in the line and thus the start of password type
	delim = strchr(current_line, ':');
	if(delim == NULL){
		goto fail;
	}

	// read the password type from the line
	current_requirement.type = strtol(delim+1, NULL, 10);
	if(errno == EINVAL){
		goto fail;
	}

	// find the password length (note : strRchr)
	delim = strrchr(current_line, ':'); // cannot fail, or we would've gotoed fail before the first time.
	current_requirement.length = strtol(delim+1, NULL, 10);
	if(errno == EINVAL){
		goto fail;
	}

	*pass_req = current_requirement;
	return 0;

fail:
	fprintf(stderr, "Corrupted syntax in config file.\n");
	return -1;
}

int remove_website_requirement(const char* domain){

	int err;
	int website_line_number;

	if( (err = get_website_config_line(domain, &website_line_number, NULL)) == 1)
		return 1; // line not found
	else if(err < 0)
		return -1; // file or syntax error.
	_dprintf("Website line to be replaced : %d\n", website_line_number);

	const char config_file[] = "./websites.lst";
	const char tmp_file[] = "./websites.lst.tmp";
	FILE* config = fopen(config_file, "r+");
	FILE* tmp = fopen(tmp_file, "w");
	if(config == NULL)
		goto fail;

	char* current_line = NULL;
	int current_line_number = 0;
	size_t current_line_size = 0;
	while(getline(&current_line, &current_line_size, config) >= 0){

		// we skip the target line
		if(++current_line_number == website_line_number){
			_dprintf("Skipping line %d during copy.\n", current_line_number);
			continue;
		}
		
		//if(strlen(current_line) < 3) continue; Not sure actually.........
		fprintf(tmp, "%s", current_line);
	}
	fclose(config);
	fclose(tmp);
	if(rename(tmp_file, config_file) == 0)
		return EXIT_SUCCESS;
	else
		fprintf(stderr, "Error while removing old website config.\n");

fail:
	fprintf(stderr, "Missing or corrupted syntax in config file.\n");
	return EXIT_FAILURE;
}

int main(int argc, char* argv[]){

	if(argc < 2){
		print_usage();
		return EXIT_FAILURE;
	}

	password_requirement_t pass_req = { .type = TYPE_ALL, .length = 10};
	bool edit_website = false;
	bool default_config = true;
	#define IF_DEFAULT_RESET do{ if(default_config){ default_config = false; pass_req.type = 0; } }while(0);
	int index;
	int c;

	opterr = 0;
	while ((c = getopt (argc, argv, "dUlnXxsL:")) != -1){
		switch (c){
			case 'd':
				debug = true;
				break;
			case 'U':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_UPPER;
				edit_website = true;
				break;
			case 'l':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_LOWER;
				edit_website = true;
				break;
			case 'n':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 'x':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_LOWER;
				pass_req.type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 'X':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_UPPER;
				pass_req.type |= TYPE_NUMBER;
				edit_website = true;
				break;
			case 's':
				IF_DEFAULT_RESET
				pass_req.type |= TYPE_SPECIAL;
				edit_website = true;
				break;
			case 'L':
				pass_req.length = strtol(optarg, NULL, 10);
				default_config = false;
				if(errno == EINVAL){
					fprintf(stderr, "Invalid password size specified.\n");
					print_usage();
					return EINVAL;
				}
				break;
			case '?':
				if (optopt == 'L')
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
				fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return 1;
			default:
				print_usage();
				return EINVAL;
		}
	}
	index = optind;

	char *domain = trim_url(argv[index]);

	_dprintf("Options : type:len = %u\%un", pass_req.type, pass_req.length);

	printf("Domain : %s\n", domain);

	password_requirement_t old_pass_req;
	int err = get_website_requirements(domain, &old_pass_req);
	if(err == 0){ // site already in memory
		if(edit_website && default_config == false){ // We supply different requirements than the onses saved
			do{
				printf("Replacing password type:length for website %s  %u:%u -> %u:%u ? (y/n) ", domain, old_pass_req.type, old_pass_req.length, pass_req.type, pass_req.length);
				c = getchar();
				putchar('\n');
			}while(c != 'y' && c != 'n');

			if(c == 'y'){
				if(	remove_website_requirement(domain) == EXIT_SUCCESS && 
					register_website_requirements(domain, pass_req) == EXIT_SUCCESS)
					printf("Website config updated. ");
				else{
					fprintf(stderr, "Error while updating website config. Aborting.\n");
					exit(EXIT_FAILURE);
				}
			}else
				printf("Change aborted.\n");
		}else{
			printf("Website already registered. ");
			pass_req = old_pass_req;
		}

		printf("Using password req : %u:%u\n", pass_req.type, pass_req.length);
	}
	else if(err == 1){
		if(register_website_requirements(domain, pass_req) == EXIT_SUCCESS)
			printf("Website config inserted\n");
		else{
			fprintf(stderr, "Error while inserting website config. Aborting.\n");
			exit(EXIT_FAILURE);
		}
	}
	else
		return EXIT_FAILURE;

	char master_pass[100] = {'\0'};
	char* output = calloc(pass_req.length + 1, 1);

	while(strlen(master_pass) == 0)
		strcpy(master_pass, getpass("Master password : "));

	gen_pass(master_pass, domain, output, pass_req);
	printf("Password : %s\n", output);

	return EXIT_SUCCESS;
}
