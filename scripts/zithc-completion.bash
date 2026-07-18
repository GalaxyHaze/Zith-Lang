# bash completion for zithc

_zithc() {
    local cur prev opts cmds
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    cmds="build run execute check fmt create deps test docs repl clean"

    opts="-h --help --version -m --mode -o --output -I --include -A --assets --check -i --in-place --emit --target --sysroot --emit-tokens --emit-ast --emit-hir --emit-ir --emit-asm --emit-all --interpreted --opt-level --debug-level -s --strict --lto --strip-debug -c --color -v --verbose"

    # Specific option completions
    case "$prev" in
        -m|--mode)
            COMPREPLY=( $(compgen -W "debug dev release fast small" -- "$cur") )
            return 0
            ;;
        --emit)
            COMPREPLY=( $(compgen -W "ast hir ir asm obj bin" -- "$cur") )
            return 0
            ;;
        -c|--color)
            COMPREPLY=( $(compgen -W "auto on off" -- "$cur") )
            return 0
            ;;
        -o|--output|--sysroot|-I|--include|-A|--assets)
            if declare -F _filedir &>/dev/null; then
                _filedir
            else
                COMPREPLY=( $(compgen -f -- "$cur") )
            fi
            return 0
            ;;
    esac

    # Complete commands or general options
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    else
        # Search if any command is already specified
        local cmd_found=""
        local word
        for word in "${COMP_WORDS[@]:1:COMP_CWORD-1}"; do
            if [[ " $cmds " == *" $word "* ]]; then
                cmd_found="$word"
                break
            fi
        done

        if [[ -z "$cmd_found" ]]; then
            COMPREPLY=( $(compgen -W "$cmds" -- "$cur") )
        else
            # We are inside a command, e.g. "zithc build <file>".
            # Provide file/directory completion
            if declare -F _filedir &>/dev/null; then
                _filedir
            else
                COMPREPLY=( $(compgen -f -- "$cur") )
            fi
        fi
    fi
}

complete -F _zithc zithc
