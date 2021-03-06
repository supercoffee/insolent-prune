#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/errno.h>
#include "functions.h"
#include "base64.h"
#include "bcrypt.h"

#define INT_BUFFER_LENGTH 16

#define ERROR_NON_NUMERIC_INPUT 1
#define ERROR_INT_OVERFLOW 2

#define HASH_BUF_LEN 255

/*
  Determine if path1 is a subpath of path2.
  Path2 must contain the entirety of path1 for this to happen.
  // TODO FIX THIS
*/
int compare_file_paths(char * path1, char * path2){

  size_t path1_len = strlen(path1), path2_len = strlen(path2);

  // Path1 cannot be a subpath if it's shorter than path2
  if (path1_len < path2_len){
    return 0;
  }

  uint32_t i;
  for (i = 0; i < path2_len; i++){
    if (path1[i] != path2[i]){
      return 0;
    }
  }
  return 1;
}


/*
https://codereview.stackexchange.com/questions/37177/simpler-method-to-detect-int-overflow
*/
int safe_add(int32_t a, int32_t b, int * error) {

    int32_t result;

    int overflow = __builtin_sadd_overflow(a, b, &result);
    if (overflow){
      *error = overflow;
    }
    return result;
}

int safe_multiply(int32_t a, int32_t b, int * error){

    int32_t result;
  // New in GCC5 to check for overflow when multiplying.
  // Why did this take 30 years to get into a standard library?
  int overflow = __builtin_smul_overflow(a, b, &result);
  if (overflow){
    *error = 1;
  }
  return result;
}


/*
    Returns 0 if passwords match.
    Returns -1 if password does not match stored hash
    Returns 1 if password file cannot be opened
*/
int verify_password_from_file(char * password){

  FILE * password_file = fopen(PASSWORD_FILE, "r");
  if (NULL != password_file){


    // Grab the base64 encoded hash from the passwords file
    char stored_hash[BCRYPT_HASHSIZE];
    read_string_from_file(stored_hash, BCRYPT_HASHSIZE, password_file, NULL);
    fclose(password_file);

    return bcrypt_checkpw(password, stored_hash);

    //
    // // Convert base 64 encoded PW hash into binary string
    // int stored_hash_num_bytes = Base64decode_len(stored_hash);
    // char * stored_hash_bytes = (char *) calloc(stored_hash_num_bytes, sizeof(char));
    // Base64decode(stored_hash_bytes, stored_hash);
    //
    // // Hash the user's submitted password
    // unsigned char hash[crypto_hash_BYTES];
    // crypto_hash(hash, (unsigned char *) password, strlen(password));
    //
    // // Compare hashes using constant time function to prevent timing attacks
    // return crypto_verify_32((unsigned char *) stored_hash_bytes, hash);
  }
  return 1;
}

int write_password_to_file(char *password) {

    FILE *password_file = fopen(PASSWORD_FILE, "w");
    if (NULL != password_file) {
        store_password(password, password_file);
        fclose(password_file);
        return 0;
    }
    return 1;
}

int store_password(char *password, FILE *output) {

    if (NULL != output) {

      char salt[BCRYPT_HASHSIZE];
      char hash[BCRYPT_HASHSIZE];
      bcrypt_gensalt(12, salt);

      bcrypt_hashpw(password, salt, hash);

      output_to_stream(hash, output);

        // unsigned char hash[crypto_hash_BYTES];
        // // Invoke NaCl hash function (based on sha512) to hash the pw securely
        // crypto_hash(hash, (unsigned char *) password, strlen(password));
        //
        // // Determine how long the hashed version is
        // // and how long the base 64 version will be
        // size_t hash_length = strlen((char *) hash);
        // int encoded_length = Base64encode_len(hash_length);
        //
        // // Allocate a buffer large enough for the Base 64 version of the hash
        // char *encoded_hash = (char *) calloc(encoded_length, sizeof(char));
        // Base64encode(encoded_hash, (char *) hash, hash_length);
        //
        // output_to_stream(encoded_hash, output);

        return 0;
    }
    return 1;
}

void output_to_stream(char *output, FILE *stream) {
    fprintf(stream, "%s", output);
}

int seek_to_newline(FILE *stream) {

    int ch, ctr;
    for (ctr = 0; (ch = getc(stream)) != EOF && ch != '\n'; ctr++);

    return ctr;
}

void read_string_from_file(char *buffer, int length, FILE *input,
                           const char *prompt) {

    if (NULL != prompt) {
        printf("%s", prompt);
    }

    if (NULL != buffer && NULL != input) {
        fgets(buffer, length, input);
        // Replace newline character with null byte.
        // https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input

        // Not ideal, but because fgets will never read more than INT_MAX,
        // size_t can be downcast to an int safely
        int chars_read = (int) strcspn(buffer, "\n");

        // If no newline occurs before the end of the string, then it must be
        // sitting on the buffer and needs to be removed.
        if (chars_read == length - 1) {
            seek_to_newline(stdin);
        } else {
            // Set newline character to null because fgets leaves it there
            buffer[chars_read] = '\0';
        }

    }
}


/**
    Convert a string into an integer.
    Caller must check error parameter to ensure
    that conversion result is accurate. The value zero is
    returned in error cases.
*/


int32_t str_to_int(const char *in, int *error) {
    char *end_ptr;

    long long_val = strtol(in, &end_ptr, 0);

    // check errno to see if the number is outside the range of long
    if ((errno == ERANGE && (long_val == LONG_MAX || long_val == LONG_MIN))
        || (errno != 0 && long_val == 0)) {
        *error = ERROR_INT_OVERFLOW;

        return 0;
    }

    // Check if any chars were thrown away by strtol
    if (*end_ptr != '\0') {
        //The input contained some non numeric garbage,  abort
        *error = ERROR_NON_NUMERIC_INPUT;
        return 0;
    }

    if (long_val > INT_MAX || long_val < INT_MIN) {
        *error = ERROR_INT_OVERFLOW;
        return 0;
    }

    *error = 0;
    return (int) long_val;
}
