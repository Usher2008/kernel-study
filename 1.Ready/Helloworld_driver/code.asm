.code

; breakpoint 函数返回参数加 1 的值
breakpoint proc
    int 3
    ret
breakpoint endp

end