# gen-pass (for lack of a better name)

Simple personnal password generator with password type memory for each website/service.

The idea is to generate unique passwords for websites using their domain name and a master password.
I was just too lazy to remember a gazillion passwords, and not trustful enough towards popular password managers (no tinfoil hat though).

The issue this tool attempts to addresse is that many websites impose silly requirements for passwords. (size, character types).
I must admit I have done no research at all to find a tool doing this specific job. But this gave me the opportunity to make this little guy.

I guess this solution is far from perfect. But hey, it works for me.

## Usage : 

    ./gen-pass [-dUlnXxs] [-L pass_size] sitename.com
    
    -d : debug mode (NIY)
    -L [pass_size] :
         Size of the output password.
    
    Character classes : (default : Xxs)
    
    -U : enable uppercase letters
    -l : enable lowercase letters
    -n : enable number characters
    -X : enable uppercase letter and number characters
    -x : enable lowercase letter and number characters
    -s : enable special characters
    
## Saved websites :

When specifying no character classes, the program uses a default password type. If the domain name is known, the password type is loaded from the config file.
Otherwise, a password is generated using the default password type, and an entry is added in the config file.

When specifying one or more character classe/s, the program checks if the domain name is known. If so, the configuration for that domain is overwritten (after confirmation).
Otherwise, a password is generated using the the specified character classes, and an entry is added in the config file.

## Contributions :

Feel free to contribute ! I'd be glad to see someone actually interested in this little thing.
