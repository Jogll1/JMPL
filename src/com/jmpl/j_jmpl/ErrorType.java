package com.jmpl.j_jmpl;

public enum ErrorType {
    SYNTAX,
    TYPE,
    VARIABLE,
    ZERO_DIVISION;

    private final String name;

    ErrorType() {
        // Format name
        StringBuilder s = new StringBuilder();
        String typeName = this.name();

        // Format each word in sentence case
        for(String w : typeName.split("_")) {
            s.append(Character.toUpperCase(w.charAt(0)) + w.substring(1).toLowerCase());
        }

        name = s.append("Error").toString();
    }

    public String getName() { return name; }
}
